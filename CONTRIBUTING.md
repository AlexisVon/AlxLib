# Contributing

Thanks for wanting to help. Two rules keep this library what it is: the test suites stay green,
and the public headers stay usable from C++11.

## Reporting bugs

Open an issue with a minimal reproducer -- a compile line or a test fragment beats a description.

## Building and testing

```bash
cmake -B build -DALXLIB_OPT_LEVEL=3 -DBUILD_TESTS=ON -DBUILD_EXAMPLES=ON
cmake --build build -j7
cd build && ctest --output-on-failure
```

## Style

- C++17 for the library's own sources; the public headers stay C++11-clean (see the README).
- Comments in English, and only where the code cannot say it itself -- one crucial line. If it
  needs an essay, the naming or the structure is what wants fixing.
- Match the surrounding style: the tree is `clang-format`-clean under the config shipped in
  `.clang-format`.
- Tests live in `gtest/<module>/src/`, one `gt_<name>.cpp` per unit; a change without a test is a
  change half made.

## Sign your commits (DCO)

This project uses the Developer Certificate of Origin (see `DCO`) instead of a CLA. Sign every
commit off:

```bash
git commit -s
```

which appends a line like:

```
Signed-off-by: Your Name <you@example.com>
```

Signing off certifies the statements in `DCO` -- briefly: the contribution is yours to submit,
under this project's license (MIT, inbound = outbound).
