# Copyright (c) 2026 AlexisVon
# SPDX-License-Identifier: MIT

# test_py.py — single-file Python encode/decode for cross-language testing
#
# Usage:
#   python3 test_py.py encode <out.bin>   write standard test data to binary file
#   python3 test_py.py decode <in.bin>    read binary, verify all fields, exit 0 on pass

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "python", "bin"))
import alxbase

TEST_DATA = {
    "bool_true": True,
    "bool_false": False,
    "int_small": 42,
    "int_neg": -128,
    "int_zero": 0,
    "int_large": 123456789012345,
    "float_val": 3.141592653589793,
    "string": "hello alxbase",
    "string_empty": "",
    "unicode": "你好世界",
    "null_val": None,
    "bytes_val": b"\x00\x01\x02\xff\x80",
    "list_mixed": [1, "two", True, None, 3.14],
    "list_nested": [[1, 2], [3, 4]],
    "nested_map": {"a": 1, "b": {"c": "deep"}},
}


def deep_equal(actual, expected, path=""):
    if type(actual) is not type(expected):
        # allow int/float cross-comparison for int_small etc.
        if isinstance(actual, (int, float)) and isinstance(expected, (int, float)):
            if actual != expected:
                print(f"MISMATCH at {path}: {actual!r} vs {expected!r}")
                return False
            return True
        print(f"TYPE MISMATCH at {path}: {type(actual).__name__} vs {type(expected).__name__}")
        return False

    if expected is None:
        if actual is not None:
            print(f"MISMATCH at {path}: {actual!r} vs None")
            return False
        return True

    if isinstance(expected, bool):
        if actual != expected:
            print(f"MISMATCH at {path}: {actual!r} vs {expected!r}")
            return False
        return True

    if isinstance(expected, int):
        if actual != expected:
            print(f"MISMATCH at {path}: {actual!r} vs {expected!r}")
            return False
        return True

    if isinstance(expected, float):
        if actual != expected and not (actual != actual and expected != expected):
            print(f"MISMATCH at {path}: {actual!r} vs {expected!r}")
            return False
        return True

    if isinstance(expected, str):
        if actual != expected:
            print(f"MISMATCH at {path}: {actual!r} vs {expected!r}")
            return False
        return True

    if isinstance(expected, bytes):
        if actual != expected:
            print(f"MISMATCH at {path}: {actual!r} vs {expected!r}")
            return False
        return True

    if isinstance(expected, list):
        if len(actual) != len(expected):
            print(f"LIST LEN at {path}: {len(actual)} vs {len(expected)}")
            return False
        for i, (a, e) in enumerate(zip(actual, expected)):
            if not deep_equal(a, e, f"{path}[{i}]"):
                return False
        return True

    if isinstance(expected, dict):
        if set(actual.keys()) != set(expected.keys()):
            print(f"DICT KEYS at {path}: {set(actual.keys())} vs {set(expected.keys())}")
            return False
        for k in expected:
            if not deep_equal(actual[k], expected[k], f"{path}.{k}"):
                return False
        return True

    print(f"UNSUPPORTED TYPE at {path}: {type(expected).__name__}")
    return False


def do_encode(path):
    buf = alxbase.encode(TEST_DATA)
    with open(path, "wb") as f:
        f.write(buf)
    print(f"encoded {len(buf)} bytes → {path}")


def do_decode(path):
    with open(path, "rb") as f:
        raw = f.read()
    obj = alxbase.decode(raw)

    fail = 0
    for key, expected in TEST_DATA.items():
        if key not in obj:
            print(f"MISSING: {key}")
            fail += 1
        elif not deep_equal(obj[key], expected, key):
            fail += 1

    if fail:
        print(f"{fail} FIELD(S) FAILED")
        sys.exit(1)
    print(f"decode OK: {path} ({len(raw)} bytes)")


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: test_py.py encode|decode <file>", file=sys.stderr)
        sys.exit(1)
    cmd, file = sys.argv[1], sys.argv[2]
    if cmd == "encode":
        do_encode(file)
    elif cmd == "decode":
        do_decode(file)
    else:
        print(f"Unknown command: {cmd}", file=sys.stderr)
        sys.exit(1)
