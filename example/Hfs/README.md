# Hfs — HTTP file manager example

One page, one process: browse the host filesystem over `alx::http`, select an item to
inspect it in a persistent right-hand pane, open folders, view text and images in a
large overlay, download in 4 MB blocks, upload and delete. Light theme by default, a
dark theme and an English/Chinese toggle in the header. No external fonts, CDNs or JS
libraries.

## Scope

This example targets **private LAN file exchange**. It is built for practicality and
stability, **not for security**:

- no authentication, authorization or access control
- no TLS; plain HTTP only
- without a `root_dir` the API reaches anywhere the process can; with one, `/` maps to
  that directory and web paths containing `.` or `..` segments are rejected
- uploads are capped by the HTTP server's 64 MB request-body limit
- downloads are assembled in browser memory from 4 MB blocks; very large files can
  exhaust the tab

Run it on a trusted network only.

## Build

From the repository root:

```bash
cmake -B build -DALXLIB_OPT_LEVEL=3 -DBUILD_TESTS=ON -DBUILD_EXAMPLES=ON
cmake --build build -j7
```

`Hfs` lands in `bin/`, with the page assets copied to `bin/Hfs.page/` beside it.
To build only this example:

```bash
cmake -B build-hfs -DBUILD_TESTS=OFF -DBUILD_EXAMPLES=ON \
      -DBUILD_EXAMPLE_SCPT=OFF -DBUILD_EXAMPLE_LZA=OFF -DBUILD_EXAMPLE_HFS=ON
cmake --build build-hfs --target Hfs -j7
```

## Run

```
Hfs                              # 127.0.0.1:8080, virtual root
Hfs 192.168.1.10                 # 192.168.1.10:8080, virtual root
Hfs 192.168.1.10:9000            # explicit port
Hfs 127.0.0.1:8080 /data/share   # serve /data/share as "/"
```

The first argument is always the address; the second, when present, is an existing
directory used as the file-manager root. A missing or non-directory root is a startup
error (non-zero exit). With no root, the virtual root lists common directories on POSIX
and drive letters on Windows; uploads are disabled there because it is not a real
directory. With a root, `/` is a real directory and accepts uploads, while the root
itself cannot be deleted.

Type `exit` and press Enter to stop; with no interactive stdin the process keeps
running until Ctrl+C.

## UI

- A single click selects; the right pane shows the item's type, size, modified time,
  location and a small preview (or a folder / type card).
- Open with a double-click, Enter or the pane's Open button: a folder navigates, a
  text or image file opens the large viewer, any other file downloads. The viewer
  closes with Esc, the backdrop or the close button; the selection stays in the pane.
- The pane can be hidden with the toolbar toggle or its own close button; the choice is
  remembered. Nothing selected shows the current folder summary and its item count.
- Delete and overwrite confirmations show the full path in a monospace block before the
  action. Delete removes files and empty folders only.
- The status bar shows the item count and the current selection.
- Keyboard: Up/Down select, Enter opens, Backspace goes up, Esc clears the selection or
  closes the viewer.

## Routes

| Method | Path | Notes |
|--------|------|-------|
| GET | `/` | `index.html`, also served at `/index.html` |
| GET | `/app.css`, `/i18n.js`, `/api.js`, `/ui.js`, `/app.js` | static assets |
| GET | `/filesys/root` | `{"virtual":bool,"name":"…","path":"…"}` |
| GET | `/filesys/list?path=` | directory listing as JSON |
| GET | `/filesys/read?path=` + `FileBlockIndex` header | one 4 MB block, with `FileBlockIndex` / `FileBlockTotal` reply headers |
| POST | `/filesys/upload?path=&name=[&overwrite=1]` | raw body as the file content, 64 MB cap |
| DELETE | `/filesys/remove?path=` | file or empty folder |

Errors are JSON `{"error":"code"}` with a matching HTTP status; the page maps the code
to the current language.

## Resources

All static assets live in `page/` and are copied to `bin/Hfs.page/` at build time by an
always-run target. `src/res.cpp` is the only place that knows where they are: it
resolves the directory next to the executable (falling back to `./page`), and exposes

```cpp
void res::init(const char* _argv0);
bool res::load(const std::string& _key, alx::bytes& _out);
```

Handlers call `res::load()` and never touch the filesystem for page assets. To move the
assets into a Win32 resource section later, replace `src/res.cpp` with a
`FindResource` / `LoadResource` implementation of the same two functions and add a
`.rc` file to the target; nothing else changes. Editing a page file requires a rebuild
so the asset copy runs.

## Behaviour notes

- Folders are listed first; Name / Size / Modified are sortable.
- Text preview reads the first 4 MB block; the pane renders 64 KB and the viewer 1 MB,
  with a truncation note. Images up to 16 MB are shown from their blocks with their
  pixel dimensions; larger images show a type card and a download button.
- Upload sends the raw body with the filename in the query string. A name collision
  returns `exists`; the page asks, then retries with `overwrite=1`.
- Symlinks are skipped in listings and refused by read, upload and delete.
- On Windows, paths follow the process ANSI code page: `file_info` and `fopen` use the
  A APIs, and the root argument arrives through `argv` in the same code page. Web paths
  are converted to it per request and refused if they contain a backslash.
