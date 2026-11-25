# alxbase — Known Limits and Pitfalls

Two parts: **§A Open defects** (confirmed, not yet fixed) and **§B Limits and pitfalls** (edge cases outside the behavioural contract, not counted as defects). Every entry states its trigger and its check, so a limit can be tightened later on demand.

---

## §A Open defects

No open defects at present.

---

## §B Limits and pitfalls

## Memory and ownership

- **A failed `bytes` allocation throws**: a size that cannot be represented throws `std::length_error`, a malloc that fails throws `std::bad_alloc` (`null()` means empty, not failed). Uncaught in the host it reaches `terminate`; thrown on a thread of the library's own -- the send and receive threads of `comm`, say -- it kills the whole process. See the entry of the same name in `doc/comm/notice.md`.
- **`bytes::resize(0)` has two semantics**: with a shared block (reference count > 1) it goes through `clear()` and drops the block; with an unshared one it only sets the block header's `size` to 0 and **keeps the allocation**. `resize(0)` therefore promises no release of memory; use `clear()` / `fitsize()` for that.
- **`bytes::data()` (the non-const one) detaches**: called on a shared block it deep-copies. Taking `data()` on the same shared object over and over on a hot path is one O(n) copy each time; call `detach()` first, or cache the pointer once.
- **`bytes_view` holds the underlying `bytes` by value** (not a raw pointer): the view keeps the block alive until it is destroyed; `data()` answers `nullptr` only while the underlying `bytes` is `null()` (a zero-length view over a live block still hands out a valid pointer), and `operator[]` is unchecked.
- **`bytes::to<T>()` checks no size**: `r_interpret<T>` overlays a T straight away, so bounds and alignment are the caller's business; the payload is allocated with `default_align = 4`, i.e. 16-byte alignment.

## Containers and types

- **`varmap` iterates in key order** (its storage is `map_btree`, ordered by `operator<`), which is not insertion order. Do not rely on a walk when insertion order is what is wanted.
- **`varmap::insert` replaces the value of a key that is already there**: it answers the entry's iterator, with no `{it, bool}` pair — under replace semantics the flag says nothing the caller acts on. `try_emplace` is the keep-the-first form: a known key leaves both the stored value and the argument untouched and answers `{existing, false}`. `take(key)` is the removal that hands the value over by move (an absent key answers a null variant), so nothing is leaked and nothing needs freeing.
- **A duplicate key in JSON text keeps its last occurrence**: the parser inserts each pair and the object's insert replaces, so `{"a":1,"a":2}` parses to `a = 2`; the object itself never holds duplicates.
- **`varmap`/`varvec`/`varlst` are deep-copy value types**: assignment and copy construction duplicate every element; inside, `varmap` is a `map_btree<std::string, variant*, true>`, so every value is its own heap allocation while the entries live packed in B-tree nodes.
- **A numeric `variant::to<T>()` converts only when the conversion is lossless**: a cross-type read (`int` → `uint`, `long long` → `int`) is decided by `is_lossless<FROM,TO>`, and a lossy one answers `_def` (T() by default) -- no throw, no truncation. `variant::to_number()` collapses to `double`, so a large integer loses precision; outside the contract -- called on a non-numeric value -- it answers **NaN** rather than 0, because 0 would pass for a legitimate answer while NaN propagates through the arithmetic that follows.
- **`json_array` derives from `std::vector`, and the base's `erase` / `pop_back` / `resize` still move raw pointers**: no element is destructed, so they leak or double-free. `clear()` and the copy/move/destructor take over the ownership (see the header comment); to change the element set, use `clear()` and `append` again.
- **`map_btree` with `IsOwnPtr = true` owns what its pointers point at**: erase/clear/destruction delete the pointee, a copy clones it (`new T(*p)` on the static type, and a null pointer stays null), an overwriting insert/emplace deletes the replaced pointer, and `try_emplace` takes nothing on a hit. insert follows the value argument's category: an lvalue `Value` or entry is copied (an owning map clones the pointee, the caller keeps its pointer), an rvalue one is adopted and its source pointer is left null; `emplace` adopts whatever pointer it is constructed with -- handing the map a pointer another owner still holds is a double free. `take(key)` removes an entry while handing its pointer over undeleted, which is the way to move a value out. The `Value&` from `operator[]` is not intercepted -- a pointer assigned through it leaves the old pointee for the caller to delete. `varmap` / `json_object` use this mode.
- **Every `json_array` element is `new`ed**: appending one `json_value` costs one heap allocation, so a large number of small elements is worth a `reserve` first.
- **`map_btree` invalidates more than `std::map` does**: inserting a new entry or erasing shifts entries inside a node and moves them between nodes on a split or merge, killing iterators and any reference or pointer into an entry (overwriting a known key leaves the entries in place; a plain map keeps the pointed-to object, while an owning map deletes the replaced pointee, so references to the old value die). Code that leans on `std::map`'s "an insert never invalidates an iterator" must re-find after a mutation. Check: no `varmap`/`json_object` call site holds an iterator across an insert, so the narrower contract changes no existing behaviour.
- **`map_btree::insert` replaces the value of a known key, and an owning map deletes the replaced pointee**: a reference to the old value (`varmap::value()` included) dies with it. `try_emplace` is the keep-the-first form. Check: `varmap` / `json_object` are owning maps, and `__varmap_impl::insert` replaces as well.
- **`map_btree::value_type` keeps the key non-const**: assigning through `it->first` compiles and silently breaks the ordering invariant; a key must not be modified once inserted.
- **`map_btree::erase(iterator)` copies the next key when a successor exists**: the successor is found again after the rebalance, one key copy plus one lookup (erasing the last entry copies nothing); use `erase(key)` when the successor is not needed.
- **`map_btree` element requirements**: `Key` and `Value` must be default constructible, copyable and movable (a node keeps an array of `value_type`), and `Key` must be ordered by `operator<`. A type missing one of these fails to compile inside the node, not at the call.
- **`map_read` is sparse and read-first**: live entries plus tombstones stay within [1/8, 1/4] of a power-of-two table, so memory runs several times a dense map's; `clear()` releases the table instead of keeping a block for reuse; an erased slot keeps a tombstone that the probe skips and a later insert reuses, and `erase(iterator)` answers the next occupied slot, so a conditional delete during iteration is safe; it never resizes, so a drained map keeps its slots until `clear()`. A mostly-tombstoned table pays a little more per probe until a rehash clears it (half-dead: +0.16 ns). There is no `cannot_contain`.
- **`conver<To>` does not interpret ownership**: a raw-pointer mapped type is handed over as pointer values, and the move form clears the source without deleting anything. An owning map on either side is a compile error -- detach or consume it first.
- **`map_hash` invalidates on every mutation**: any insert or erase may demote entries between the two slot zones and their chains, and a rehash rebuilds the table, so iterators and references die on either. Its walk (L0 slots, then each L1 slot followed by its chain) is an implementation detail, never an ordering promise.
- **`map_hash` is lazy and address-keyed for pointer keys**: a constructed map owns nothing until the first insert or `reserve`; `clear()` releases the arrays and returns it to that lazy state, so a reservation must be redone after a clear. A pointer `Key` -- a `const char*` included -- is stored, compared and hashed by address; use `std::string`. `value(key, default)` is the non-inserting read on every map.
- **`map_hash`'s Hash must keep its low and high 32-bit halves independent**: the low half homes an entry, the high half places it after a demotion. `hash::auto_64` is the default (wy_64 through 64 B, ah_64 above); `fnv_64` answers the same interface but not the independence requirement. A chain holds at most 4 entries before a rehash, and a floating-point NaN key can never be found (it never equals itself).
- **`map_hash` with `IsOwnPtr = true` follows `map_btree`'s ownership contract** (the same third template argument): erase/clear/destruction delete, copies clone, an overwriting insert deletes the replaced pointer, and `take` hands the pointer over undeleted. Its std conversions are the plain (non-owning) forms only, for the same two-owner reason as `map_btree`'s.
- **`map_hash::cannot_contain(key)` is one-sided**: `true` means the key is certainly absent; `false` means only that absence was not proven — never read it as presence (use `contains`). It is a structural probe with no key comparison, and it exploits the six-bit hash fragment kept in each L0 slot's flag byte; a `Hash` whose keys all share that fragment simply turns the probe conservative and restores the full key comparison in `find`.

## Hash (`ahash.h`)

- **A hash value is not stable across machines or builds**: the AES-NI path and the scalar path answer different values for the same input, and `ALXLIB_AHASH_NO_HW=ON` recompiles to the scalar one. Never persist a hash value or compare values across builds/processes; within one build the function is deterministic. Check: `is_hard()` tells which path a running build uses, not which path produced a stored value.
- **`is_hard()` needs both the compiled path and the CPU**: it answers false in a scalar-forced build, and on a CPU without AES-NI a default build also answers false and falls back. The fallback is a different function, not a slow version of the same numbers.
- **`ah_64`'s integer and byte primitives differ**: `hash(x)` and `hash(&x, 8)` answer different values. `fnv_32` / `fnv_64` instead hash an integer as its little-endian bytes. Pick one form per key domain and keep it.
- **`func::eval(0)` hashes the integer zero**: the literal resolves to the arithmetic adapter (exact match), not to a null `const char*`; hash an empty string as `""`.
- **Floating point hashes by bit pattern**: `0.0` and `-0.0` differ, NaN payloads included; a numeric domain that must see them as equal has to normalise before hashing.
- **The hash is not cryptographic**: a 64-bit non-cryptographic hash has collisions within the birthday bound (n²/2^65), and an adversary can craft them; its job is bucket/slot distribution, never integrity. The collision-free sweeps (`gt_ahash`'s 1M-key sample, `tools/ahash_bench.cpp`'s exhaustive and per-length samples) are evidence, not a guarantee.
- **FNV's constexpr walk reads bytes as `uint_8`**: the pre-migration `fnv1a_*` cast each `char` (sign-extending bytes ≥ 0x80), so its constexpr and runtime paths disagreed; do not reintroduce a `char` cast in a constexpr hash.

## Serialization (aserial)

- **`r_interpret(ptr, size)` truncates a non-multiple length rather than reporting it**: `count = size / sizeof(T)`, and a tail too short for one T is dropped silently. A blob whose length field does not match its type is not refused; it is merely under-read.
- **Short data on the decode path leaves the default silently**: with `_dat.size < sizeof(T)`, `deserer_var_base_ordinary` does **not write** the target variant (it keeps the default-constructed value) and throws nothing. A field that never decoded and a field whose value happens to be the default are therefore indistinguishable in the result.
- **Only a direct call to `ser::deserer<...>::parse` needs the initialization**: the type handlers it needs are registered by the library-internal `init_core()`, a static that no header declares; the public entries (`varsolid::to_varmap` / `get_value` and the rest) run it on the way in. Calling `parse` without it answers a null variant every time -- a common source of false negatives in a check.
- **A path component of `varsolid::get_value` resolves as a decimal index**: solid does not transcode strings -- values are raw bytes -- and the path walk only does the positional parse. An index that is not valid (empty, non-decimal, overflowing) answers `_def` as the contract says.

## Singletons and global state

- **The singleton `single<T>::instance()` hands out is not destroyed at process exit**: the implementation is a function-local `static T* obj = new T();`, deliberately never destroyed -- another static destructor may still be logging. A resource the singleton holds (a file, a thread) is therefore not promised to be released at exit. It also means the initialization is C++11 once-only, with no data race.
- **The default logger of `global_logger` is built on first use**: when several threads log for the first time at once, a CAS picks one winner and the losers release their own copy, so there is only ever one default logger instance.

## Other

- **`strutil::split` with an empty delimiter cuts into single bytes**: it returns `len + 2` elements (one empty element before the first and after the last, one per byte in between), not an empty list.
- **The `regex_ex` copy assignment is `noexcept`**: assigning the internal `pattern` can in theory throw `bad_alloc`, which then terminates (vanishingly unlikely, left as it is).

## Encryption and digest

- **Ciphertext that carries no padding block when the plaintext was already aligned is not guaranteed to unpad**: `ANSIX923` and `ISO10126` **always** append a whole block, an already aligned buffer included, and unpadding accepts `pad_len == 16` accordingly; a ciphertext written without that block carries data in its last byte, which unpadding reads as the length, so it may strip 1..16 bytes too many. Check: no compatibility is attempted -- the rule that skipped the block was not self-consistent (it appended nothing yet stripped by the last byte, so any last byte below 16 was mis-stripped), and there is no format worth migrating.
