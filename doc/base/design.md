# alxbase — Design

This document records the **design decisions, implementation points and tuning notes** of `alxbase`.

## Design principles

1. **Zero dependencies**: alxbase depends on the C++ standard library alone (`<string>` `<vector>` `<map>` `<atomic>` …). The layers above -- `alxcore` / `alxcomm` / `alxscpt` -- are all built on it, so it is the leaf of the whole dependency graph.
2. **No exceptions for control flow**: the library barely throws. Failure is expressed as "returns `nullptr` / returns `false` / returns `_def` / `null()` is true". The reason is that the data on the other side (network, file, script) produces malformed input at a high rate, and an exception path costs more than it is worth, with no control over when it fires.
3. **Value semantics outside, pointer semantics inside**: `bytes` / `varmap` / `json_object` behave like values to the caller (a copy is a deep copy), while the implementation uses a reference count or a pointer container, so memory management is never pushed onto the caller.
4. **Zero-copy first**: a read-only path always hands out a window type such as `bytes_view` / `content_meta`, materializing an owning object only when the data has to outlive the window.
5. **Demotion into base**: a facility from a higher layer may move here when it (a) is self-contained — no alx module and no third-party library is needed (the standard library, libc and compiler built-ins are fair game), (b) holds no shared mutable state and no concurrency machinery, and (c) has a converged interface — shape and contract frozen, with exposure tightening already done. The last point is why the move follows the freeze instead of triggering it: a lower layer pays more for an interface change, not less.
6. **C++11 floor**: `BUILD_TESTS=ON` asks for C++17 (gtest), while library code stays usable from C++11 (`autility.h` fills in the `_v` trait family for the older standards).

---

## 1. `bytes`: a copy-on-write byte buffer

### 1.1 Block layout -- one allocation

```cpp
struct bytes_block {          // block header
    ref_count ref;            // reference count
    uint_64   size;           // logical length
    uint_64   alloc;          // bytes allocated, rounded up to the alignment
    // uint_8 data[] sits right behind it
};
```

Points:

- **Header and payload sit in one piece of memory** (`malloc(align(size) + sizeof(bytes_block))`, with `data()` located by `(uint_8*)this + sizeof(bytes_block)`). Set against a control block separate from the data, that saves one allocation and one level of pointer indirection, with a better cache locality.
- **Alignment is fixed at 16 bytes** (`default_align = 4`, an exponent): it gives the reinterpret in `to<T>()` a basic alignment guarantee, at the price of up to 15 wasted bytes per block.
- **`size` lives in the block header** -- the key constraint of COW: `size` is **shared by every alias**. Any operation that changes `size` has to detach first.

### 1.2 Reference count

`ref_count` is a three-state atomic counter: `unsharable` (no sharing, `ref()` returns false), `static_ref` (a static object, never destroyed) and an ordinary count (`is_shared()` = count > 1).

- Copy construction / copy assignment: `ref()` raises the count and **copies no data**.
- Destruction: `free` only when `deref()` returns false (the count reached zero).
- Move: the pointer is simply taken over and the source is nulled.

### 1.3 Where detach fires

`detach()` = "deep-copy an exclusive one while the block is shared". Three kinds of trigger:

1. **The non-const `data()`**: once a writable pointer is handed out the block must be exclusive, or the write lands on another alias.
2. **`resize()` / `clear()`**: the header's `size` has to change.
3. **`append()` / `ensure_capacity()`**: the capacity has to grow, or the tail rewritten.

> `resize(0)` forks again: shared it goes through `clear()` (detach and release), unshared it only sets `size` to 0 (keeping the allocation).

### 1.4 Growth strategy and failure semantics

- **1.5x growth**: `ensure_capacity` runs `while (_need > new_size) new_size += (new_size >> 1);`. 1.5 rather than 2 is what makes a block freed after several growths more likely to be reused; a 2x step leaves freed blocks too big to be reused.
- **`realloc` first**: an unshared block grows in place through `realloc`, and a failed one keeps the old block: the old block survives a failed realloc, so the caller keeps its data.
- **A failed allocation throws**: `bytes_block::allocate` / `reallocate` are the only allocation entry points (nothing outside `abytes.cpp` calls them), throwing `std::length_error` for a size that cannot be represented and `std::bad_alloc` for a malloc that fails, the same line STL containers take. **That makes "the old block survives a failure, no data is lost" loud**: a failed `realloc` leaves the old block untouched and the exception hands control back to the caller. `null()` means "empty", not "failed".

### 1.5 `bytes_view`: the underlying buffer held by value

The members of `bytes_view` are `bytes m_orig` (not `const uint_8*`) plus an offset and a length. That is deliberate, not a slip:

- while the window lives the block necessarily lives (the reference count backs it up), so no caller has to guarantee a lifetime;
- the price is one touch of an atomic counter on every construction, copy and destruction.

Every position-relative parameter (`find` / `equa` / `mid` …) is read as **relative to the window's first byte**; the implementation computes `m_ofst + _from` before calling down and subtracts `m_ofst` from the answer.

---

## 2. `variant`: type erasure without a vtable

### 2.1 A function-pointer table instead of virtual functions

Every supported type gets a **static function-pointer table** at compile time (`__resource_ctrl<Args...>::new_free / new_init / new_move / stk_init / opt_eqal / idx_type / def_meta`), and at run time the type index `m_type` picks the entry:

```cpp
if (m_heap) CTRL::new_init[m_type](m_dptr);
```

Set against "every value carries a vtable pointer", the gains are:

- a variant object **carries no vtable pointer** (8 bytes saved, exactly the inline threshold);
- the type index is the compile-time constant `typelist::index_of<TYPE, T>()`, so `is<T>()` / `to<T>()` degrade to an integer comparison;
- the order of the tables matches the type list one to one, so adding a type touches `typelist` in one place only.

### 2.2 Heap/stack split

```cpp
static constexpr uint_64 STACK_SIZE{8};
heap_mode<T>() = !std::is_trivially_copyable_v<T> || sizeof(T) > STACK_SIZE;
```

- **Trivially copyable and at most 8 bytes** → stored inline in `union { void* ; uint_8[8]; uint_64 }`, no heap allocation;
- everything else (`std::string` / `bytes` / containers / `anyptr`) → heap-allocated, held by `m_dptr`.

That threshold keeps every scalar type (including `long long`, `double` and pointers) out of the heap while every container type goes to it -- a simple implementation, with none of the half-hearted inlining a "small string optimization" brings.

**The shape of the slot is a constraint**: the stack slot is `uint_8[8]` on purpose (the character family, about which alias analysis assumes nothing), and a whole-slot copy goes through the union member `m_dval`. Narrowing the slot to a type such as `uint_64` would let GCC's TBAA decide it cannot alias the `double` or pointer stored into it, and under `-O3` the write is then eliminated or reordered into a silently wrong value (`-O1`/`-O2` show nothing) -- `m_dval` looks like it overlaps `m_dstk[8]`, but it is not a redundant field and must not be deleted.

### 2.3 Move semantics

A heap-state move is **taking the pointer over and setting the source to `m_type = -1`** (the source becomes null, so its destructor frees nothing); a stack-state move copies the 8 bytes by value and nulls the source the same way. A moved-from source is therefore always `null()` -- a tighter promise than the standard "valid but unspecified state", and an easy one for a caller to test.

### 2.4 Lossless conversion policy

```cpp
template <typename FROM, typename TO>
TO to_number_impl(const TO& _def) const noexcept {
    return alx::is_lossless<FROM, TO> ? (TO) r_interpret<FROM>(...) : _def;
}
```

`is_lossless` is decided at compile time (the same type always qualifies; a signed source needs a signed target at least as wide, or a real target strictly wider; an unsigned source needs an unsigned target at least as wide, or a signed or real target strictly wider; a real source needs a real target at least as wide), and at run time only the type index dispatches. **A lossy conversion always answers the default, never a silent truncation** -- deliberate: a `variant` carries "a value shared between the host and the script", and a silent truncation turns into a business error that is hard to track down.

### 2.5 The split between `to` and `as`

| | Types match | Types differ |
|---|---|---|
| `to<T>(_def)` | the stored value comes back | `_def` comes back, **this object is untouched** |
| `as<T>()` | the stored value comes back | **`T{}` is stored** and its reference comes back |
| `as<T>(_ifnot)` | the stored value comes back | **`_ifnot` is stored** and its reference comes back |

A read path uses `to` (no side effect); a "take or initialize" path uses `as`.

### 2.6 Exception safety: commit order and the `noexcept` specification

The heap branch has to allocate, so **assignment may throw `bad_alloc`**. Two rules:

**Commit last** -- in `operator=` the statements writing `m_heap` / `m_type` have to come **after** the operation that may throw:

```cpp
if (_value.m_heap) m_dptr = CTRL::new_init[_value.m_type](_value.m_dptr);  // throws here
else m_dval = _value.m_dval;
m_heap = _value.m_heap;
m_type = _value.m_type;
```

`destroy()` has already run, so after a throw the object rests at `m_type == -1`: a **legal null state**, not the torn state of "claims to be a type whose pointer is null" (where `is<T>()` is true and one read segfaults). The semantics are the **basic guarantee** (a failure leaves the object null), not the strong one (a failure leaves the old value in place) -- the strong guarantee needs old and new side by side, which doubles the peak on a host under a tight `RLIMIT_AS` and so fails more often, not less; a bad trade.

A constructor needs no such order: a throw leaves the object unfinished and its destructor never runs, so no half-built state is observable, and copy construction / move construction still assign the state flags first.

**The null state is expressed by the tag alone**: while `m_type == -1` the payload is undefined, and no read point ever looks past the tag at it -- default construction writes the tag only, and `destroy()` puts the tag back to -1 (the release happens only when a type was stored), so wiping the payload would be neither necessary nor right.

**`noexcept` covers the stack branch only**:

```cpp
__variant_impl(T&& _value) noexcept(!heap_mode<std::decay_t<T>>())
```

The stack branch is 8 trivially written bytes and genuinely cannot throw; the heap branch does allocate, and marking it `noexcept` would promise to land `bad_alloc` as `std::terminate`, out of the host's `try/catch` reach. `noexcept(false)` lets the exception travel, so the host can treat it as a resource error.

---

## 3. `varmap` / `json_object`: pointer container, value-semantics interface

`__varmap_impl<MapType, Type>` derives from `MapType<std::string, Type*>` (`protected` inheritance, so only the controlled interface is exposed), yet behaves to the caller like a `map<string, Type>`:

- copy construction / assignment: delegated to the owning map's copy (**a deep copy**, one pass, cloning every value);
- `erase` / `clear` / destruction: `delete` the value pointer;
- `value(key)` on a miss answers the shared static default `def_val()` (a function-local static: free to reach and thread-safe);
- `operator[]` is "create a default when the key is absent", `insert` replaces the value of a known key and `try_emplace` keeps it -- the two insert forms make the intent explicit, which rules out the hidden "`[]` inserts on the way past" behaviour. `take(key)` is the removal that hands the value over instead of deleting it — the moved-out value on `varmap` / `json_object`, the pointer on the container.

**Why pointers at all**: `variant` and `json_value` may each contain the other recursively (a `variant` inside a `varmap`, a `json_value` inside a `json_object`), and a by-value member needs the type complete; a pointer breaks that cycle and keeps a map node a fixed size (no node carries the size of a whole variant).

> The price: one heap allocation per stored value and one pointer hop per read. Enough for what this library uses them for (configuration, protocol packets, JSON).

---

## 4. JSON / XML

### 4.1 JSON

- **Parsing is recursive descent** (the `syn_json*` family), over a lexical layer `lex_json` that advances byte by byte. **Check the bound before reading** is a hard rule: while `_next == _size` the code must never read `_data[_next]`.
- **`json_value` reuses the type erasure of `variant`** (`__variant_utils_base`, which takes the scalars and the two containers only), so a read goes through a wrapper such as `to_intg` / `to_string` with an `_ok` out-parameter rather than the `to<T>()` template -- the read semantics of JSON are "a type mismatch has to give the caller an explicit failure signal", and `_ok` says that more plainly than a default value does.
- **Integers all become `long long` and floats `double`**: the widening happens in the constructors, so `int` / `long` do not produce different type indexes across 32- and 64-bit platforms.
- **`bytes` goes Base64 automatically**: JSON has no binary type, and encoding is the only lossless choice.

**The ownership model of `json_array`**: inside it is a `std::vector<json_value*>`, **raw-pointer storage rather than `vector<json_value>`**, at the price of filling in every lifetime operation by hand:

| Operation | Semantics |
|---|---|
| Destruction / `clear()` | `delete` every element |
| Copy construction / copy assignment | deep-copy the elements (copy assignment goes through copy-and-swap, exception safe) |
| Move construction / move assignment | `vector::swap` takes them over; move assignment `clear()`s itself first |
| `operator==` | compares the values element by element (the inherited vector version compared the **pointers**) |
| The inherited `erase` / `pop_back` / `resize` | **still raw-pointer operations that know nothing of ownership** (noted in the header) |

Why the raw pointers stay: it matches the incomplete-type plus fixed-node-size constraint of `variant` / `json_value`, and the change is small. The price is the table above -- **any new interface that removes some of the elements has to carry the deletion semantics explicitly**.

**The two conversion routes (copying / moving)**: the moving one **hands the heap buffers of `std::string` / `bytes` / containers over one by one** (no copy), leaving the source an empty shell (same type index, no content) that **may only be destroyed afterwards**.

| Direction | Copying | Moving |
|---|---|---|
| variant ↔ json_value | `from_variant(const variant&)` / `to_variant() const` | `from_variant(variant&&)` / `take_variant()` |
| Container level | `from_varmap` / `from_varvec` / `from_varlst` / `to_varmap` / `to_varvec` / `to_varlst` (`const&`) | the `&&` overloads of the same names; the way back is `take_varmap` / `take_varvec` / `take_varlst` |

- **Why the reverse is named `take_*` and not `to_variant() &&`**: giving a parameterless const member a `&&` qualifier first forces `const &` onto the existing one ([over.load]: ref-qualifiers must be present on every declaration sharing a parameter list or on none), and the symbol then changes from `_ZNK…` to `_ZNKR…` ⇒ **an ABI break**. Another name costs nothing, and the name itself says "take it away and leave it empty" (as `mmap::take_all()` does).
- **Why the forward direction can take a `&&` overload outright**: a static function is told apart by its parameter list (`const variant&` vs `variant&&`), so no existing symbol moves.
- **The two that cannot be moved**: `bytes` needs Base64 (the encoding necessarily allocates a fresh copy) and a stack-inlined scalar has no heap block -- both fall into the copying path, with the same semantics (including "a type outside the set gives a null `json_value`").
- **The contract**: after a throw the source is "half moved" and may still only be destroyed; `std::move` is the caller's declaration (the source is dead), and an lvalue still selects the copying version.

**The two routes of serialization (`json_doc`)**: the `const&` forms of `to_json` / `to_string` only read the input; the `&&` forms **eat the tree as they write it** (each entry is handed back the moment its text is out).

| | How | Peak |
|---|---|---|
| Copying | append layer by layer into the caller's string (RVO builds the returned string in place, no object copy) | `tree + text` |
| Moving | the same, plus removing each entry from its parent as soon as it is written (object `erase(key)`, array deletes the element and empties the slot, string `swap`s itself back to the system) | on the order of `max(tree, text)` |

- **One implementation only**: the 6 writer templates of `alx::json_doc_impl::writer` (a namespace plus a static class, **inside the .cpp**, never in a header) take `NODE&&` -- **const-ness travels with the argument**: a read selects the `drop()` no-op overload and a mutable argument the one that really releases. **Whether to eat is not a switch, it is the type**, so the two routes cannot drift apart.
- **Leaf strings**: `text()` bulk-appends in runs (breaking only at a character that needs escaping), rather than building the two whole-string temporaries of the `to_string()` copy → `stringfy` → `+=` chain.
- **Two places have to hand things back one at a time**: `std::string::clear()` does not shrink (the memory is not returned) and `json_object::clear()` would delete what has not been written yet; removing from the front of a `vector` is O(n²), so an array deletes elements by index and empties the slot, and only `clear()`s at the very end.
- **Measured** (probe: `ru_maxrss`, 64 MB of data): a single leaf 196 → 196 MB (**the `&&` form adds nothing** -- the whole string has to live until the write is done), 32x2 MB 194 → 132 MB (the tree shrinks as it is written, absorbing even the moment the output grows).
- **The sink need not be a string**: the writer templates take `_SINK&`, and a sink only needs `append(const char*, size_t) -> bool`. Two instantiations -- `std::string` (always true, so the check folds away; **in-place/aliasing** resize plus backward write is something only it can do) and `alx::ostream` (the host writes straight into storage it owns: a mapped block, a socket, a file; `append` returning false means "it does not fit", the walk stops right there and `to_json` returns false. **A zero length is not "does not fit"**: what `escape_into` pushes is "the source text after the previous escape point", and for an empty string, for a first character that needs escaping, and for two escapes side by side that run is 0 bytes, which a sink has to accept). **The default path is not `ostream&`**: that would put a virtual call in every append, and the JSON exit is the hottest one there is. The JSON exit can therefore inline end to end -- `writer::text` calls the internal `escape_into`, not the exported `json_doc::escape` (which crosses the PLT and cannot be inlined; that one point is the whole of the 8~13% by which `to_json` beats the other way of writing it).
- **The landing contract of `escape` / `descape`**: `(src, dst, ofst)` -- the first `ofst` bytes of `dst` are **left alone**, the result starts at `ofst`, and `dst` is resized to `ofst + result length`; `src` may be `dst` itself (then `ofst` is 0 and the path is count plus backward write, the write side always above the read side). The escape set follows RFC 8259: `"`, `\` and U+0000-001F (the short forms `\b \f \n \r \t`, the rest `\u00xx`), keys and values alike.

### 4.2 XML

- **`content_meta` is "a union plus a type tag"**: `ELEM` (an `xml_value*`) and the family of string kinds from `STRI` up (`TEXT` / `NOTE` / `PI__` / `DTD_` / `CDAT` / `ENTY`) share one `void*`. `free()` / `copy()` dispatch on `type_` to destroy or deep-copy, and destruction is **recursive** (destroying an element takes its subtree with it).
- **An element is a name plus an attribute table plus a content list**: attributes in an `unordered_map` (order is of no interest), content in a `list<content_meta>` (order kept, insertion and walking stable).
- **No name may repeat in the attribute table** (`from_bytes_attr` refuses the attribute when the key is already on the node): that is what keeps the parse free of the ambiguity where a later same-name attribute overwrites an earlier one. The parser tolerates malformed input poorly (it would rather fail), because XML in this library carries configuration and documents, where not being parsed is safer than being parsed wrong.

### 4.3 CSV

- **The table is just a `varvec`**: the outer level is rows and the inner one cells, with `row 0` the header row -- no new table type is invented, and there is no "document object" as in `json_doc` / `xml_object`; the entry points are free functions under `alx::csv` (the same shape as `strutil`).
- **The entry takes a raw byte range** (`const char* + uint_64`), with `std::string` an inline thin wrapper in the header -- the same shape as `json_doc::from_value` (`include/alxbase/ajson.h:432`): a `bytes`, an `mmap` or a socket buffer can be fed straight in, with no container built first (`csv` has no `bytes_view` overload while `json_doc` does; a `bytes` in the caller's hand gives up `data()`/`size()` anyway, so one more overload would buy no new case). The price is that reading a value takes two hops (`rows[r].to<varvec>()[c].to<std::string>()`), because `variant` does not index -- that is the bill for "no new table type".
- **The output side follows the json family**: `to_bytes` has a returning form (`bytes`) and a streaming one (`ostream&`, where an `append` returning false means "it does not fit" and the walk stops right there), each with a borrowing version and an `&&` consuming one (a row handed back per row written, so the peak is max(table, output) rather than their sum). Inside, the returning form wraps a `bytes` in an `ostream_buff` (`include/alxbase/astream.h:159`) and shares the one stretch of writing logic with the streaming form.
- **The read contract**: a cell is always `std::string`; RFC 4180 quoting (`"` around it, `""` for one literal quote, the delimiter and line breaks allowed inside); `\r\n` / `\n` / `\r` all end a row, and the last row may omit its break; **a blank line is not a record** (a row that opens and ends at once is skipped -- "a single empty field" and "a blank line" cannot be told apart in the text, and skipping is the reading Go takes, which keeps strict mode from being killed by a blank line at the end of a file); whitespace outside quotes is data (nothing is trimmed); row 0 is kept as it stands. **An unclosed quote, junk behind a closing quote, a quote inside a field that was not opened with one, and a delimiter outside the range (= quote / line break / `\0`) all fail the whole table** -- the loose mode relaxes the field count and that dimension alone.
- **Strict / loose** (`_strict`): strict (the default) requires every record to carry as many fields as row 0 held, else the whole table fails; loose pads and truncates nothing, so each row keeps its own field count and the matrix may be ragged.
- **Encoding**: a first pass looks for any high byte -- plain ASCII (the common case for a text csv) is parsed as UTF-8 directly, **zero-copy, never disturbing iconv**; only with a high byte present does `detect_format` (`source/alxbase/astring.cpp:108`) probe: **UTF-16 is converted as a whole** (the delimiter is two bytes there and cannot be cut cell by cell); **any other non-UTF-8 (GBK and the like) is split on the raw bytes and converted cell by cell** (`source/alxbase/acsv.cpp:211`) -- a cell that cannot be converted fails the whole table, no half of it is handed out. **Input holding one BOM alone is an empty table** (not a failure).
- **The delimiter has to be ASCII** (`< 0x80`, and not `"` / CR / LF / NUL): the splitting happens on the raw bytes, and only a trailing-byte range of a multi-byte encoding (0x40-0xFE for GBK) that does not overlap the delimiter cuts cleanly.
- **Writing takes text cells only**: a cell must hold a `std::string`, else the whole call fails (an empty `bytes` / `false`) -- the module does not pick a float format for the caller, because the library's `%.6f` with trailing zeros dropped (`source/alxscpt/script/ascript_utils.h:102`) is for **display**, and writing a data format with it would lose precision silently; a numeric cell is the caller's to format, or a later addition can follow the shortest round-trip form of `source/alxbase/ajson.cpp:38`.
- **A quote goes on only when it is needed**: the cell holds the delimiter, a quote or a line break, or its first or last byte is a space or lower (plenty of readers trim). Two special cases: **the only cell of a row, when empty, is written `""`**, and **a row with no cells at all is written `""` too** -- otherwise that line is a blank line in the text, skipped on the way back in, and the data vanishes.
- **No algorithm layer**: no sorting, grouping or joining. Over a plain text matrix "sort" has no single right answer (lexicographic versus numeric), so picking one in the library gives the other half of the cases a **silent wrong answer** (a downstream host has a comment on a test case recording exactly this trap); a comparator callback in C++ has already been judged "not worth it" on that side too. An unambiguous structural helper (locating a column by its header, say) waits until there is a real sticking point.
- **No input size cap**: the entry takes the buffer the caller already holds, not a size claimed in a file header -- so there is no "forged size" attack surface here (against the compression side of `doc/core/design.md` §2.3, where a claimed size has to be clamped). The memory blow-up of the matrix (one `variant` plus one `std::string` per cell, empirically 3~5 times the text) is for the caller to weigh by row count; the library sets no threshold for it.

---

## 5. String utilities

- **`find` / `rfind` dispatch on the pattern length** (`source/alxbase/aalgo.cpp:27-34`, and the same table for rfind): 1 / 2 / 4 / 8-byte patterns go to a whole-value comparison (read point by point as `uint_8/16/32/64`); the rest at 8 bytes or fewer in a window under 512 go to brute force; 16 bytes or fewer in a window under 512 to Rabin-Karp; longer, or a larger window, to Boyer-Moore. The reason for the dispatch is that searches mostly happen on **protocol boundary scans** (looking for `\r\n\r\n`, looking for a delimiter) where short patterns are the overwhelming majority, and there a value comparison or a brute-force match beats a general algorithm on a short window. `find_by_kmp` is still in `aalgo.h`, with no caller at present.
- **Two interfaces, "raw pointer + length" and `std::string`**: the core implementation knows only `(ptr, len)`, and the `std::string` version is a thin wrapper. That way a zero-copy case over a `bytes_view` / `bytes` reuses it directly, with no `std::string` built first.
- **`format` uses `%1 %2 …` placeholders** (not printf style): it avoids runtime errors such as a format string that does not match its argument types, and every argument is handled as a `std::string`.
- **Code conversion**: `to_wstring` / `from_wstring` / `code_conver` go through the platform APIs (on POSIX the locale/iconv family, on Windows the `MultiByteToWideChar` family), and `detect_format` probes the source encoding from the BOM and the byte distribution.

---

## 6. Binary serialization

### 6.1 Layers

```
serable    -- element types and data type definitions (data size / stack-only / type id)
serer      -- encode: write the head (type + name + data size) → payload → tail
deserer    -- decode: walk the metadata → dispatch to the handler per type
```

The format is **self-describing**: every node carries a type id and a data size, so the decoding side needs no schema. Type handlers are registered in the three tables `deserer::CORE_BASE` / `CORE__VEC` / `CORE_LIST`, dispatched by the `BIT16` type id. The three are `map_read<BIT16, COREFUNC>` (13-14 handlers each): built once at first use through `conver`, and dispatched with `peek` -- one hash and one tag byte per node instead of a node walk.

### 6.2 Path extraction `varsolid::get_value`

The goal is **to reach one field without decoding the whole thing**: locate the data area straight from `serer::HEAD`, then parse the metadata node by node (a map matched by key, a vec/list counted by index) and decode the node that hits. It suits "read a few fields out of a large configuration packet".

- A type id that does not match, a path segment that is not valid, and an index out of range all answer `_def` as the interface promises (**no exception**).
- The model of `varsolid`: **solid does not transcode strings -- every value is raw bytes** -- and the path step is a positional parse: a path component is used as a decimal index in the index branch.
- Numeric keys are parsed by a hand-written bounded accumulation: an empty string, a non-decimal one and an overflow all answer false, so the "answers `_def`" contract holds without an exception escaping. **`variant::select` uses the same bounded accumulation.**

### 6.3 Boundary discipline on the decoding side

Serialized data may come from outside (a disk, a network), so the decoding side treats **metadata as untrusted**:

- `r_interpret(ptr, size)` **truncates rather than over-reading** a non-multiple length (`count = size / sizeof(T)`, the trailing remainder dropped);
- list decoding stops when the space left is short of one whole element (half an element is never read);
- a fixed-width scalar is checked for `size >= sizeof(T)` before decoding, and a short one **stays at its default** and is not written into the target.

---

## 7. Factory and self-registration

`factory<TYPE, Args...>` holds a static `name → creator` table; `product<IMPL, TYPE, Args...>` is a static registrar whose static member `_register` is constructed during **static initialization** and registers `IMPL` with the factory on the way.

- The name registered is `typeid(IMPL).name()`: mangled under GCC/Clang and restored with `abi::__cxa_demangle`, while under MSVC the `typeid` name is taken as it is with the `class ` / `struct ` prefix cut off. `create("circle")` therefore looks up the **demangled, fully qualified name**: namespaces are kept as they are (on the MSVC side only the `class ` / `struct ` prefix is cut). The library's own implementations all sit in the global scope, so an implementation's name is its class name; a host that puts its implementation class in a namespace registers and looks it up under the namespace prefix.
- The gain: a new implementation is one class to write, with no registry to edit; the price: the registry is free of static initialization order (each table is a function-local static), but **the linker may drop an object file nothing references**, so a scenario that creates by name has to make sure the object file holding the implementation class is linked in.

---

## 8. Trade-offs in the other facilities

| Facility | Decision | Rationale |
|---|---|---|
| `signal` | many reads, few writes: a `shared_mutex` to emit under and a `unique_lock` to connect under; below C++17 it degrades to a `mutex` | an emission is the hot path (message dispatch) and a connect the cold one |
| `single<T>` | function-local `static T* obj = new T();`, **never destroyed** | C++11 guarantees the initialization to be once-only and thread-safe; it is not destroyed because another static destructor may still be logging (`global_logger` is one such user) |
| `ref_count` | three states (unsharable / static / ordinary count) | a "static object" has to be expressible as "never destroyed", or a static `bytes` breaks on destruction |
| `anyptr` | a handle of deleter plus cloner plus reference count | it lets a `variant` carry an arbitrary host object while keeping value semantics |
| `noncopyable` | copy deleted, **move kept** | combining it with base classes such as `auto_delete` / `object` needs no extra move operations written |
| `object` | links to its parent at construction and unlinks at destruction | a tree (GUI widgets, AST nodes) needs no hand-kept unlink bookkeeping |

---

## 9. Behavioural invariants

| Topic | Rule | Reason |
|---|---|---|
| `bytes::resize` | detaches before changing `size` on a shared block | `size` lives in the shared block header, so not detaching pollutes every alias |
| `aserial` read points | the read/write points go through `m_interpret` (`memcpy`) and its writing overload | a serialization buffer sits at arbitrary offsets, so reading an `uint_32`/`uint_64`/`HEAD` straight off it is an unaligned access (34 UBSAN reports); the reading `m_interpret` also goes through an `alignas` scratch buffer, which covers a type with no default constructor (a BMP header) |
| `aserial` nameless node | `swrite_head` skips the `memcpy` when the length is 0 | the caller passes null for an empty name, and `memcpy(p, nullptr, 0)` is UB (UBSAN reports `null pointer passed as argument 2`) |
| `bytes` allocation entry points | `allocate` / `reallocate` test for an "impossible size" first (both the `1 << _align` step and the `+ sizeof(bytes_block)` step are tested for wraparound); the 1.5x growth of `ensure_capacity` tests for wraparound; `bytes::reallocate` / `ensure_capacity` return the block unchanged when it is null | a wrapped-around `malloc` argument is a small value while the block header records the pre-wrap amount, so the object **lies about** a `size()`/`capacity()` of the order of 2^64 (measured: `bytes(2^64-24)` is non-null and `capacity()` reports 2^64-16) |
| `json_array` | destruction / deep copy / move / value comparison all handle the elements | a raw-pointer vector has no ownership of its own: elements leak and copies are shallow |
| `json` lexer | `lex_json` checks the bound on entry | reading before checking walks one byte past the end while `_next == _size` |
| `varsolid` numeric keys | a hand-written bounded accumulation | `stoull` throws on an empty string and on an over-long number, contradicting the contract |
| `single<T>` | a function-local static, not `call_once` plus a raw-pointer fast path | a raw-pointer fast path reads while the write happens inside `call_once`, so two threads taking the singleton for the first time race |
| `xml` attribute parsing | the cursor is committed only on success, and a missing `=` fails at once | after a failure the cursor has already moved, which makes a caller retry off by one and misread `<a />` as an attribute |
| `regex_ex` copy assignment | `delete regx` before the assignment | overwriting the pointer leaks the previous compiled regex |
| `strutil::split` | an empty delimiter is defined as "cut into single bytes" | without that rule it falls into a boundary branch of `find`, with an undefined result |
| `bytes::append` | throws when it cannot grow (`length_error` / `bad_alloc`) instead of carrying on into the `memcpy` | carrying on after a failed allocation writes out of bounds, while a silent early return cannot tell "nothing appended" from "an empty append" |
| `json` conversion | moving entries (`from_*(&&)` / `take_*`) | const overloads alone leave two representations alive at once while converting a large value (a host hits its memory wall) |
| `json` serialization | the private writers go append-style; the `&&` entries eat the tree as they write | a leaf string would be copied twice and every subtree would build one more temporary string; serializing a large value would have to feed the tree and the text both |
| `variant::select` | index parsing is a bounded accumulation: an empty key, a non-decimal one and an overflow all answer `_def`, as the contract says | `std::stoull` throws on an empty string and on an over-long number, contradicting the contract that "any segment that does not resolve answers `_def`" (the same rule as the path parsing of `aserial` / `varsolid`) |

## 10. Encryption (AES)

### 10.1 The three-layer structure

```
aes_base          the algorithm core: key expansion, the round functions, GHASH, padding (stateless, pure functions)
  └ aes<128|192|256>   holds round_key, defines key_len / round_num
      └ aes_iv<...>        adds the IV and its backup (for reset)
          ├ aes_cbc
          └ aes_ctr
      └ aes_gcm            carries its own counter/ghash state, protected inheritance (the core stays hidden)
```

A work mode is **a template plus virtual functions**: the bit size is fixed at compile time (`key_exp_size` is a constant, `round_key` an on-stack array, with no heap allocation) and the mode lives in the polymorphic layer.

### 10.2 The soft / hard dual implementation

`cipher` / `inv_cipher` / `ghash` each come in a `_soft` and a `_hard` version, and each probes the CPU's capabilities at run time to pick one (`aes_base::hardcal()` reports AES-NI alone; GHASH is decided by a separate PCLMULQDQ probe). The eight-block `cipher_blocks` and `inv_cipher_blocks` pairs follow the same split. `TRY_AES_HARD` controls whether the hard path and the 16-byte aligned key array are enabled at all; with it off both probes can only answer false and every `_hard` entry point is an empty body.

**GHASH's hard path computes in the reflected domain**: GCM numbers a block's bits from left to right, the leftmost being the coefficient of `y^0`; reversing the bit order of every byte (leaving the byte order alone) turns it into the plain polynomial "bit `q` is the coefficient of `y^q`", with the same sparse modulus as before, `y^128 + y^7 + y^2 + y + 1` -- the 128x128-bit product is assembled with PCLMULQDQ, and the high 128 bits fold back into the low ones with two passes of a multiplication by `0x87`. Both the reflection and its inverse happen inside `ghash_mul_hard`, so the two hardware entry points are semantically word-for-word identical to `ghash_soft` / `ghash_mul_soft`; they are **private**, like the `cipher_impl_hard` family, and reachable only through the self-probing `ghash()` -- a public entry point cannot carry a precondition such as "the CPU must have PCLMULQDQ" that the caller has no way to check.

**Batching.** `aes_ctr` and `aes_gcm` drive an eight-block batch: `cipher_counter_blocks` fills eight key-stream blocks from the counter (each takes the current value, then the counter advances) and enciphers all eight in one pass, so one block's AES-NI latency hides behind the other seven. The counter is advanced as two big-endian 64-bit halves; a byte-wise carry loop costs about 140 cycles per batch through a store-forwarding stall (measured).

`ghash_blocks` folds four GHASH steps at a time: `s' = (s ^ d0)·H⁴ ⊕ d1·H³ ⊕ d2·H² ⊕ d3·H`. The four multiplies are independent, so the fold is throughput-bound instead of latency-bound; the per-message power table `H²…H⁴` is computed once by `ghash_powers` and cached in `aes_gcm` (`gcm_pow`, 48 B).

`aes_ecb`, and the decrypt side of `aes_cbc`, use the same eight-block pass (`cipher_blocks` / `inv_cipher_blocks`); the inverse pass precomputes the `aesimc` round keys once per batch instead of once per block. CBC encryption is a chain and stays one block at a time.

> **The reflected subkey is not cached**: `ghash_blocks` reflects `h` once per eight-block call, not once per block, and the figures include that. Measured over 64 MiB blocks with `tools/aes_bench.cpp` (interquartile mean of 15 samples, a fresh mode object per operation): CTR 4339 MiB/s, GCM 2223 MiB/s. The same pass carries ECB and CBC decryption: ECB 8866 / 6353 MiB/s (encrypt / decrypt), CBC decrypt 5236 MiB/s, while CBC encryption stays serial at 1209 MiB/s. The published vectors and fpacker's `V1OutputIsStable` pass.

### 10.3 The semantics of the padding modes

`buf_padding` / `buf_unpadding` are **public API** (brought in with a `using` in `aes<N>`), because the caller needs to pad before encrypting and strip after decrypting.

| Mode | What is padded |
|---|---|
| `PKCS7` | every byte is the pad length |
| `ZEROS` | fills 0, **nothing appended when already aligned** |
| `ANSIX923` | the first n-1 bytes are 0 and the last byte is the length; a whole block is appended when already aligned |
| `ISO10126` | this implementation fills `0xff` (the standard asks for random) and the last byte is the length; a whole block is appended when already aligned |

> `ANSIX923` / `ISO10126` always append a whole block when aligned, as the standards say: unpadding strips by the last byte, and an aligned buffer's last byte is data itself, so it cannot double as a pad length -- any value below 16 would be mis-stripped.

### 10.4 The CTR and GCM counters

`aes_ctr` is a **stream**: `iv_index` records the offset inside the current block and carries across several `xcrypt` calls; the counter is incremented as one big-endian number (the carry walks from the last byte backwards). `encrypt` / `decrypt` share `xcrypt`.

`aes_gcm` keeps J0 / the counter / the GHASH state in the object; the three-part `start(aad)` → `encrypt`/`decrypt` → `finish(tag)` guarantees that both the AAD and the ciphertext go into GHASH, and `finish` runs `reset()` on its way out (so the object is reusable). The counter at rest is the next **unused** value -- the constructor and `reset()` leave it at `J0 + 1` -- so the batch and the scalar tail share one rule (take the counter, then advance). A message may be fed in pieces, and **every piece but the last has to end on a 16-byte boundary**: a trailing partial block is padded and folded into the tag right away, which closes the block stream -- only `finish()` may follow, and any further `encrypt`/`decrypt` call returns false. The guard reads the low nibble of `gcm_proc_len`, so it adds no state.

---

## 11. Digest

- **CRC is parameterized by template**: `crc_base<CRC, POLY, INIT, XORO, INRE, OURE>` makes the polynomial, the initial value, the output xor and the input/output reflection all compile-time parameters, and `CRC_32` and `CRC_32C` are just two sets of them. The lookup table is built lazily on first use by `static const bool initialized = [] { init(); return true; }();`.
- **SHA-1 / SHA-256 have a hardware path**: after probing for SHA-NI they run `__m128i` instructions (holding the round constant table and the mask table), and otherwise fall back to the pure software implementation; `hardcal()` lets the caller ask which of the two is in use.
- **One factory**: the algorithms self-register into the `verify` factory through `alx::product`, `list()` gives the registered names, and `create(enum)` / `create(name)` are the two ways in.
- **HMAC is a verify type, not a parallel interface**: `hmac` extends `verify` with `init()` and its product `hmac_sha_256` lives in `averify.cpp` on the same engine, so the soft and SHA-NI paths are shared and `hexdigest()` / `bytedigest()` keep verify's meanings. `init()` precomputes the two padded states and snapshots them; each message starts from a copy, and neither the key block nor the padding block re-enters the per-message path. `clear()` restores the keyed preset for the next message, as a hash needs before being fed again.
- **PBKDF2-HMAC-SHA256 lives in `aalgo.cpp`** (`alx::pbkdf2_hmac_sha256`): it stands on the public `hmac` type -- each round is `update(U)` + `bytedigest()` from the keyed state, with `clear()` restoring that state between rounds, and the machine-order word dump converted to the standard octet string -- rather than on a private fast path; the SHA-256 compression dominates the round; the PBKDF2 loop allocates only the 32-byte MAC it reads back, and the derived key lands in the caller's buffer.

---

## 12. Hashing (`ahash.h`)

- **Two layers: hash sources and the `func` constraint/adapter**. A hash source is a static class `xx_yy` (`yy` = output width) implementing the primitive convention -- exactly `hash(const void*, size_t)` and `hash(uint_64)`, plus `is_hard()` and `is_constexpr()` -- and declares no result type of its own. `hash::func<Hash, Ret>` is the constraint/adapter layer: `static_assert`s fix the result type (both primitives must answer the caller-declared `Ret`) and `eval` widens the input range (NUL-terminated text, arithmetic by bit pattern, `data()/size()` types). The division is deliberate: a source only answers the convention, while every question about the result type and about what may be hashed is settled at `func` -- which is also where an unsupported input fails to compile, not inside a container. A new source joins with one alias at the call site (`using hash = alx::hash::xx_yy;`).
- **Two paths behind one call, no function pointer**: `ah_64::hash` is `if (have_hard()) hash_hard(...) else hash_soft(...)`, with `have_hard()` forwarding `cpu_has_aes()` -- a function-local static that probes CPUID leaf 1 once. When the hardware path is not compiled in, `hash_hard` forwards the scalar path and the branch folds away. Compile flag `-maes -msse4.1` on `ahash.cpp`; `ALXLIB_AHASH_NO_HW=ON` replaces it with `-DALX_AHASH_NO_HW`. The helpers are force-inlined, so the exported entry is the only call boundary left: measured ~3.4 ns per short-key call.
- **`wy_64` is the inline sibling**: wyhash final v4.3 (seed 0, default secret), reimplemented and answer-identical to the reference on random inputs of every length class (105,600 cross-checks); the integer primitive is `wyhash64(x, 0)`. No dispatch and no exported call -- the mixing compiles at the call site, with `__uint128_t` or the same-valued 32-bit decomposition where that is absent, so answers do not depend on the target. Its trade against `ah_64` (`tools/ahash_bench.cpp`): 0.7-0.9x on 0-16 B, parity through 64 B, 1.2-1.65x slower from 127 B up where the AES path wins.
- **`auto_64` is the default**: one compare on the length -- `wy_64` through 64 B, `ah_64` above; the integer primitive routes to `wy_64`'s inline mix. The crossover is measured, not guessed (65 B: wy/ah 1.21, 127 B: 1.35, 1024 B: 1.65). Keys of 64 B or less answer CPU-independently; a longer key follows `ah_64`'s effective path, and with AES not compiled in that path is 1.7-2.4x slower than `wy_64` on long input -- still correct, since the pair is only compared inside one process. `map_hash`'s default `Hash` is this class; the dispatch body is a compare plus a call, small enough to keep the short-key call sites inlined (the failure mode `slot_map::get` runs into).
- **The hardware path uses `AESENC` as a mixer, not as a cipher**: the data enters as the round input (not as the XOR key), because one round spreads a byte over its column and three rounds cover the block -- that is the ≤16 B case. Longer input runs two accumulators over 16-byte blocks, absorbs the overlapping last 16 B, and folds with two more rounds; the length enters the seed, so padding collisions cannot occur.
- **The scalar path segments by length** (0 / 1-3 / 4-7 / 8-16 / >16 with a 16-byte loop and an overlapping tail) and funnels every branch through one avalanche (xor-shift + multiply); the short paths must complete the full avalanche because the multiply's high bits depend on the low ones.
- **The two paths answer different values, deliberately**: nothing persists a hash, and the map only compares within one process. Both paths are covered by the same test suite through the build option (default build = AES path, `ALXLIB_AHASH_NO_HW` build = scalar path), green on both.
- **Quality is a gate, not a hope**: `gt_ahash.cpp` checks both 64-bit sources for the avalanche rate over a length matrix, the independence of the high and low 32-bit halves on 16-byte buffers (0.5 ± 0.01), and a 1M-key deduplicated collision sample (length 8, base64). The wider collision evidence lives in `tools/ahash_bench.cpp`: exhaustive sweeps and per-length samples of 1M deduplicated keys for lengths 8/16/32/64/128, run for `ah_64` and `wy_64` alike, all collision-free. `auto_64` is not gated as a hash of its own: each branch answers its source's already-gated values, and the tests pin the 64/65 B switch and the integer route instead.
- **Speed** (`tools/ahash_bench.cpp`, this machine): 1.9-2.6 ns for 0-4 B, 2.25-2.5 ns for 8-17 B, 19.4 GB/s at 64 B, 46.2 GB/s at 256 B, 58.5 GB/s at 1024 B. FNV-1a runs about 0.6-1.5 ns on the shortest keys and falls behind by 10-50x on long ones; it stays as the reference hash.
- **FNV is the reference hash**: `fnv1a_32/64` are the `fnv_32` / `fnv_64` static classes with the fixed offset basis (there is no `_h` seed parameter). The classes widen each byte to `uint_8`; a `static_cast<uintN>(char)` would sign-extend bytes ≥ 0x80 and make the constexpr and runtime paths disagree. `aanyptr`'s constexpr `rid` compiles on this form. ASCII output is unaffected.

---

## 13. Map containers: `map_btree`, `map_hash` and `map_read` (`amap.h`)

- **Why a B-tree**: `varmap` / `json_object` sit on this container, in place of `std::map` -- one node per element, one pointer chase and one allocation per element. The usage profile (tens to thousands of entries, read-mostly, few deletions, key order is a public contract) rules out hashing for the ordered case and favours a high-fanout tree: a lookup touches about one node per level and iteration walks entries in place.
- **Node form**: fixed size, fanout computed at compile time from `sizeof(value_type)` targeting 512 B; `CAPACITY` entries plus one transient overflow entry and `CAPACITY + 2` child slots (insert-first-then-split needs the extra slot while a node is overfull); `MIN = CAPACITY / 2`, so a merge of an underflowed node (`MIN - 1`) with a sibling (≤ `MIN`) leaves at most `2 * MIN` entries, and `2 * MIN <= CAPACITY` (equality only when `CAPACITY` is even), so it never overflows. `pair<string, T*>` (40 B) gives `CAPACITY = 9` (node 504 B); `pair<uint_64, uint_64>` gives 19 (504 B).
- **The 512 B target is a bench decision, not a guess**: 256 B gives a wide node only 4 entries -- 16-22% more memory than `std::map` per entry and slower point operations at small N. At 512 B the wide slot is at memory parity (81-88 vs 80 B/entry) and beats `std::map` on lookups from ~131k entries (0.79-0.81); 768 B gains a little more at large N but hurts maps that fit in one node. The sweep also ran clean under ASan/UBSan at each capacity.
- **Insert** descends by key; a known key only assigns the stored value (the `insert(key, value)` forms never copy the key on that path); a fresh entry is placed in the leaf and the parent chain splits bottom-up, tracking the fresh entry so the returned iterator stays valid. `try_emplace` runs the same descent with an empty assign, so a known key neither constructs nor moves its arguments and the answer is `{iterator, false}`; `contains` / `count` are the const lookups built on `find`. **Erase** replaces an internal entry by its in-order predecessor and rebalances with borrow/merge; `erase(iterator)` answers a valid successor by re-finding the copied next key, because a rebalance can move it.
- **The iterator contract is narrower than `std::map`'s**: inserting a new entry and erasing shift or move entries (inside a node, and between nodes when a split or merge fires), so both invalidate iterators and any reference or pointer into an entry. Overwriting a known key leaves the entries in place; a plain map keeps its pointee, while an owning map deletes the replaced one, so references to the old value -- `varmap::value()` included -- die with it.
- **No COW**: a map-level copy-on-write buys nothing here -- the `varmap` copy delegates to the owning map's copy, which deep-copies every value anyway, so a shared map would still have to clone the values; the COW that would pay (sharing the whole `__varmap_impl` body) is a separate entry about that wrapper, not about this container.
- **`conver<To>(src)` lives in a separate header** (`amap_std.h`): one free template builds a fresh target from any container's entries -- std or amap on either side -- with the target spelled as the template argument, so no overload has to guess it from a return type (which is why a `to_std`/`from_std` pair cannot cover `map_hash` and `map_read` at once: both read `std::unordered_map`). The copy form takes `const From&`; the move form takes a non-const rvalue, moves keys and values and clears the source (a std container key is `const Key` and is copied regardless). An owning map is refused on either side at compile time: the emplace-based body hands a raw pointer over as a value, so the pointees would end up owned twice (and a move would delete them with the emptied source). Targets with a `reserve` are reserved to the source size first. The header separation keeps `<map>` out of `amap.h`, which every `avariant.h` translation unit includes.
- **Bench** (`tools/map_bench.cpp`; baseline `std::map`, same key sets and operation sequences, verified element-wise through `conver<Ref>`; heap measured in a fresh child process because glibc tcache reuse hides it from `mallinfo2`): iteration 2-20x faster throughout; the narrow slot (`uint_64 -> uint_64`) wins across the board at large N (131k: hit 0.47, fill 0.63, erase 0.50, heap 38 vs 64 B/entry); the wide slot (40 B, the `varmap` shape) is parity to better at large N (131k: fill 0.91, hit 0.79, overwrite 0.87, erase 0.87, heap 83 vs 80) and 1.04-1.25x slower on point operations at N ≤ 16384. The weak spot is erase-heavy churn: 1.17x at 131k, 1.47-1.58x at 1k-16k, and 3.7x at N = 64, where whole nodes are built and freed -- the "deletions are rare" profile keeps it off the hot path.
- **Owning pointer values live in the container** (`map_btree<Key, Value, IsOwnPtr>`, default false; a `static_assert` refuses a non-pointer with the flag on): erase/clear/destruction delete the pointee, a copy clones it, and an overwriting insert/emplace deletes the replaced pointer (insert follows the value argument's category -- an lvalue value is copied by cloning the pointee, an rvalue one is adopted with its source pointer left null -- and `take` is the remove-without-deleting channel). This centralizes in one place the invariant every wrapper would otherwise carry; the wrapper delegates clear/erase/copy to the container and keeps the value accessors, `operator==`, the iterator adapters and the insert forms (which still allocate the pointee). The implementation is tag dispatch (`drop_value` / `detach_value` / `copy_entry`), so a plain map pays nothing; clear and destruction delete pointees only for the live `k < n` entries, which keeps the out-of-range slots left by split/borrow and the node `merge` deletes outright (its values already moved out) safe. `operator[]` stays a contract hole: a pointer assigned through its reference leaves the old pointee for the caller to delete.
- **The unordered sibling climbs a three-shape ladder**: the hot unordered call sites are wide slots (`string` + `variant` / `string` + index, 40-72 B) at tens to thousands of entries, read-mostly, and the script engine also holds many one-to-four-entry scopes where a hash costs more than it saves. So a map starts as **four slots scanned in order with no hash**, the fifth entry promotes it to a **bucket table** (a hash picks a bucket of four slots, each with a six-bit fragment; a full bucket doubles the table, B=2 up to 16 buckets, with no chains), and past that cap the table becomes **two zones** -- the low 32 bits home an entry in an L0 slot; an occupied home marks the slot demoted and sends the entry to the L1 slot picked by the high 32 bits, then onto that slot's chain, a single 4-entry block. The linear block, a bucket and a chain each bound a scan to four key comparisons, and every shape but the linear one pays exactly one hash. The two-zone model at its growth trigger (λ = 0.5): ~65% utilization, ~1.7 line accesses per successful and ~2.1 per failed lookup. The bucket index mixes both hash halves (`lo ^ hi`) so a weak low half alone cannot collapse every key into one bucket; the two-zone shape still needs both halves independent. Shapes only move up; `clear()` drops every entry and the arrays but keeps the 4-slot block, so a refilling frame pays no allocation.
- **Growth runs before the store**: in the two-zone shape, chain entries above 3q/4, L0 occupancy ≥ 7/8, or a chain at four entries rehash *before* the placement, so the iterator an insert answers is never invalidated by that insert. The rehash replays L0, then L1, then the chains into a table grown 2x below 64K slots and 1.5x above; a chain that cannot take the entry during the replay grows the table again rather than spilling into a second block, which is what keeps a live chain at one block. The pair is consecutive primes (q < p) and the 32-bit modulo runs through a precomputed Barrett reciprocal (one correction, ~5 cycles against the divider's 20-40), with trial division consulted only on a rehash. In the bucket shape a full bucket doubles the count, or hands over to the two-zone shape at B=16. Full hash values are not stored in the slots -- only the six-bit fragment in each slot's flag byte (below), kept to reject candidates without comparing keys.
- **In the two-zone shape the miss rule is the demoted bit alone**: a lookup that fails on the L0 slot answers when the bit is clear, whether the slot is occupied or not -- an empty slot with the bit set is reachable after erasing the L0 occupant, and the bit is cleared only by a rehash.
- **Lazy build**: a constructed `map_hash` owns nothing; the first insert allocates the four-slot linear block (one allocation, against five for a two-zone table built up front), and only the fifth entry reaches for a table. `reserve(n≤4)` just ensures that block; `reserve(n>4)` builds the two-zone arrays directly, because a bucket table cannot statically promise a growth-free fill. `reserve(n)` sizes the two zones to n/0.65 slots in total (q ≈ 0.77n, load ~1.3), and it is the lever for bulk fills: with it the fill beats `std::unordered_map`'s at every size measured, while without it the rebuild-on-growth is the one phase the hash map loses (measured: hashing the replays is only 1.4% of that — the cost is allocating the new zone arrays and moving into them, whereas `std::unordered_map` relinks its nodes and caches hashes in them).
- **Iteration and `erase(iterator)`**: the iterator is `{map, zone, index, block, block index}`; the zones are the linear block, the bucket slab, the L0 slots, the L1 slots and a chain, and the walk order is an implementation detail, never promised. `erase(iterator)` answers the successor in place -- the linear block shifts its remainder down, a bucket or L0/L1 entry clears its flag, a chain entry shifts its block's remainder down and an emptied block goes to the free list -- so it pays no key copy and no second lookup (the btree must re-find because a rebalance moves entries).
- **Copy** clones position by position without rehashing (O(n), hash-free); move construction and swap are O(1), while move assignment first releases the target's old entries. `value(key, default)` reads without inserting on both containers; the default is a shared static, so a reference kept from an absent key's answer stays valid.
- **The L0 flag byte carries a six-bit hash fragment**: bits 0-1 are the occupied and demoted flags, bits 2-7 keep the high six bits of the occupant's hash, so no memory grows and the probe touches no extra line. A fragment mismatch *proves* the occupant is a different key, so `find` and the insert probe skip the key comparison on it (63 of 64 mod conflicts rejected by an AND and a compare on a byte the probe loads anyway); a fragment match still compares the key. The same fragment makes `cannot_contain(key)` a usable negative probe -- a structural walk (home slot, then the demoted path's L1 slot and chain head) with no key comparison, answering true only when the key is certainly absent, while false means "not proven" and never presence. This is not tag-as-identity: the fragment only rejects, so no hash-collision contract is added, and a custom hash whose fragment is equal for every key merely restores the full key comparison.
- **Bench** (`map_bench`, `map_hash` against `std::unordered_map`, same slots and operation sequences, verified through `conver<Ref>`; heap counts the brk heap and mmapped regions, since the zone arrays are mmapped and `mallinfo2().uordblks` alone undercounts them): with a known size, `reserve` makes the fill beat or match `std::unordered_map` at every size measured (ratio 0.54-1.0 from 64 to 131k entries); without it the fill loses (1.6-2.4x) because a growth allocates the new zone arrays and moves every entry into them -- hashing the replays is only 1.4% of that measured, which is why the slots carry no full hash. What the container sells is the wide-slot read path: at 131k entries hit 0.62, miss 0.47, erase 0.33 and churn 0.55 of `std::unordered_map`'s time, at 86.7 against 74.3 B of heap per entry. The narrow slot only competes through the small-table ladder (0.76 and 1.33 ns at n=1 and n=4) and is still 1.2-4.7x slower on point operations once the tables are large (its key compare is already a single 8-byte read), so `std::unordered_map` keeps that niche. The bench's `--tiny` mode sweeps n=1..32 for three key shapes (equal-length hex, mixed-length script-like names, uint64) with hit, miss, insert and erase beside `std::unordered_map`; the equal-length hex shape is the linear block's worst case, since every candidate key passes the size check before the compare.
- **Coverage and the `varsolid` cache**: `varmap` / `json_object` sit on `map_btree` in the owning mode, with the byte-identical serialization regression (varsolid bytes, JSON compact and pretty) and the script demo/cover suites green. The `gt_amap` suite covers the containers (random differential runs against `std::map` / `std::unordered_map`, a weak-hash churn that forces L1 and chain traffic, a fragment-equal custom hash, owning-pointer stress, the `cannot_contain` contract, a size-by-size climb of the three shapes plus a weak-low-half climb, and the tombstone copy/swap regression), with ASan/UBSan differential smoke over the same shapes, owning values included. The cached-size map inside `varsolid`'s stream writer runs on `map_read<const variant*, BIT64>`: the size pass inserts every visited pointer and the read pass only looks those pointers up, so the 4x sparse flat table wins the build outright and stays ahead on the all-hit reads. The reserve hint keeps twice the root varmap's size clamped to [4k, 2M] entries -- `map_read` reserves four slots per entry, so the halved cap keeps the transient table near the ~130 MB bound. Measured: `to_bytes(big, 20k)` 0.295-0.301 ms and `to_bytes(big, stream)` 1.15-1.37 ms; `to_varmap`, which parses through the serial dispatch tables, is unchanged.
- **The small-table ladder**: the linear block / bucket / two-zone climb answers the small-table cost. Measured by one probe source (same key sets, 65,536 probes per sample, 30 samples, best-of): a successful lookup at n=1 costs 0.76 ns for `uint64` keys and 2.57 for 12-char `string` keys; n=2 0.89 and 3.34; n=4 1.33 and 7.05 -- the linear scan's four key comparisons are the limit, and equal-length strings are its worst case (real names differ in length and reject on the size check first); from n=5 up the two shapes are at parity (narrow 2.1-2.4, wide 5.7-6.0). Misses trade the other way: the linear block answers exactly after up to four comparisons (wide n=4: 8.70 ns), and a bucket miss costs four flag/fragment checks (narrow n=5-16: ~2.2-2.5 ns). A sweep of the bucket cap (8/16/32) puts 8 behind at n=12-32 and 16 against 32 within noise on hits, so 16 is the cap -- it bounds the bucket slab at 64 slots, where the two-zone table at the same entry count uses fewer. The script A/B (six scripts, 16 paired rounds, child CPU time, libraries swapped under one binary) is inside +-3% on every case with no systematic sign, so the ladder's win is below script granularity: the container probe is the evidence, the scripts only confirm no regression. The object is 104 B: the four counters and the two moduli fit 32 bits each (a table cannot outgrow the 32-bit modulus the reciprocal already uses), and the two reciprocals share one 24-byte block instead of two padded 16-byte ones. The 4-slot block is retained across promotions so `clear()` returns to the linear shape without an allocation, and every member carries its default in the declaration, so no constructor can miss one.
- **Capacity tuning: 4/4/4 for both key models**: T1 at 6, 8, 12 and 16, bucket slots at 2 and 8, the chain at 2, and the bucket cap at 32 are each compiled and clocked against the same probes (and a collision-heavy hash for the chain). For `uint64` keys a larger T1 buys 0.2-0.4 ns on hits at n=5-6 and about 40% off inserts at n=5-8, but misses lose 0.6-1.7 ns from n=5 on and hits lose from n=10; T1 12/16 lose on both sooner -- the numeric hash is only ~1 ns, so skipping it does not pay for a longer scan. Bucket slots at 8 is a clear miss-path loss (eight branchy flag reads per probe; n=5-8 misses 2.6-3.9 against 2.0-2.2). The chain at 2 is lookup-neutral on a sane hash (chains rarely reach four) and 0.5-1 ns cheaper only on a collision-heavy set at small n, converging at large n while buying more growth -- chains are 4, and their real cost (no fragment filter on chain entries) is a separate lever. For `string` keys T1=3 pays only on the equal-length worst case (n=4 miss 3.7 against 7.0) and costs ~2 ns on the n=4 hit with realistic mixed-length names (3.2 against 5.3), so the block is 4 there too.
- **The pointer read path**: both maps offer `peek(key)` returning `const Value*` on a const map and `Value*` otherwise, `nullptr` when absent — the read without the 40-byte iterator and the `end()` comparison. It shares the probe with `find` (one descent / one locate) and the probe is force-inlined (`__forceinline` / `always_inline`, macro released at the end of the header) so a call site inside the `.so` does not pay an out-of-line call; `find` runs through the same inlined body. The engine's hot sites use it: `data_store::find`/`contain`/`store`, `slot_map::get`, and the `m_natives` lookups in the dot resolver, the native call dispatch and the fwrap bind check. Measured: the six-script A/B (16 paired rounds, libraries swapped under one binary) sits at 0.999-1.006 paired medians, and the container probes put `peek` level with `find` (the store-shaped probe: n=4 2.73 -> 2.92, n=32 5.55 -> 5.48, n=64 6.26 -> 6.23) — in an inlinable translation unit the compiler already scalarizes the iterator, so the API is a convenience the hot paths use, not a measured engine win; the forced inlining does not regress `find` either.
- **Script-engine container selection**: `link_area::m_natives` and `slot_map`'s promoted table are `map_read` -- its probe reads in 4.2-4.6 ns against `map_hash`'s 5.3-5.9, and the take path in 5.6-7.4 against 7.7-9.1 at n=8-16; slot_map keeps its inline four-slot tier (the hand-written small table the type is meant to leave alone). The ordered `varmap` / `json_object` stay on `map_btree`. `map_hash`'s trade is on table size: multi-name scopes win (`globals16` -14~-15%, 8 locals per frame -9~-10%), tiny tables lose (single-global hot loop **+5.4%**, the per-recursion function-name lookup +2.6~3.4%). The per-lookup reason is measurable: a map_hash hit at n<=4 costs ~2x a std::unordered_map hit (~4.5-5.5 against ~2 ns -- the wy/auto hash is ~2.6 ns against the inlined Murmur's ~2, plus the zone probe and a 40-byte iterator), and the generic lookup is a real out-of-line call inside the .so (+~2 ns; forcing `find`/`locate` inline recovers it, while a hidden-visibility build changes nothing, so symbol preemption/PLT is not the cause). Running `engine_impl::m_extend` / `m_define` on `map_hash` is rejected on the same measurement: it moves `get_extend` from 4.4-7.3 to 9.0-11 ns at N=2-512 -- one vcall plus an out-of-line `locate`, with no raw win to offset it in 2-30 entry tables. `fwrap`'s method table stays `std::map`: it is iterated once at bind time into `m_natives`, never on a call path, and it sits in the public signature. Selection rule: string or pointer keys, read-mostly, tables that can exceed ~16 entries, with the lookup visible to the compiler -> `map_hash` with `auto_64`; ordered contract -> `map_btree`; `uint64 -> pointer` narrow slots, bind-time/static/cold tables, and lookups that sit behind the .so's out-of-line interface -> `std::unordered_map` / `std::map`; a table built once and only read afterwards (a static dispatch table, the root variable store, the extension registry) -> `map_read`, even behind a vcall, because its probe is one hash and one tag where the two-zone pays demotions and chains; a table whose drain goes through `erase(iterator)` (slot_map's promoted table) -> `map_read` as well, since an iterator erase never resizes and costs one tag write.
- **`map_read`: a deliberately lopsided read map**: a full member of the family -- the family interface and semantics, minus `cannot_contain` -- with the specialization only on the read path: flat open addressing over a power-of-two table, a one-byte tag per slot (0 = empty, otherwise the top seven bits of the hash plus one) packed in its own array, and a `value_type` slot array. A read hashes once, touches one tag byte per probe, and compares the key only on a fingerprint match; a miss usually stops at the first tag. Live entries plus tombstones are held within [1/8, 1/4]: an insert that takes the sum past a quarter doubles the table, and a key erase that leaves the tombstones outnumbering the live entries rehashes them away -- to a power of two of four times the live count, so the live load lands in the same band after every rehash. The sparsity is measured, not guessed -- the prototype sweep put 4x at the knee (capacity/entries 2 -> 4 saves 0.2-0.45 ns on misses; 4 -> 8 is flat) -- and it is what costs the several times a dense map's memory; the type is recommended for a finite element set built once and then read. The alternatives were measured and rejected: seed search for a collision-free placement cannot build at n >= 8 (probability exp(-(n-1)/2) per seed), and a chain of overflow entries loses to flat probing outright at load 1.0. The entry arrays stay `pair` rather than split keys/values: the family's iterator must answer `value_type&`, which a split layout cannot, and the measured SoA gain comes from the dense tags, which stay. Writes are plain open addressing with tombstone deletion: an erased slot keeps a 0xFF tag that the probe skips at the same cost as a fingerprint mismatch, and a later insert reuses it, so an erase costs one probe and `erase(iterator)` answers the next occupied slot. A tombstone is the deletion mechanism rather than a forward conditional delete: a wrap-around cluster moves a low-slot entry into a high slot, so a forward conditional delete would meet that entry a second time; the tombstone removes the defect rather than narrowing the iterator contract. Growth counts live entries plus tombstones against the quarter capacity, so a churned table keeps its probe bound, and the key-erase path rehashes once tombstones outnumber the live entries, clearing them. Measured: with half the table tombstoned a hit pays 0.16 ns over the equivalent compact table (6.75 against 6.59), with a quarter 0.05 -- read speed rests on the hash, the tag and the key compare, and the probe path does not change. Conversion goes through `conver<To>(src)`: the target is the template argument, so `map_hash` and `map_read`, which share the same `std::unordered_map` source, need no distinct names. Bench (`map_bench --read`, n = 8..512, mixed-length script names): hit 4.4-5.2 ns against `map_hash` 6.0-7.9 and `std::unordered_map` 8.0-12.5 (the 12.5 is n = 16, where its bucket rebuild dominates), miss 2.5-2.8 against 4.1-5.7; the narrow slot hits in 1.4-1.6 ns against 2.0-2.8, with a bare hash at 0.63 and a direct array read at 0.3 -- the read is the hash plus roughly one tag and a key compare. The priced weak side: fill 30-35 ns/insert and erase 28-32 ns/erase at n = 16..128 (the growth and shrink rehashes). `gt_amap` carries 65 cases -- differential churn against `std::unordered_map`, a zero-low-bits hash that forces one long probe cluster, a wrap-around cluster whose conditional delete would revisit an entry (`ReadEraseDuringScan`), owning-pointer stress, `conver` including the map's own container, the std export, and a size sweep of `peek`.

---
