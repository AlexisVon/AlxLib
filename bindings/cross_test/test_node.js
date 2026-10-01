// Copyright (c) 2026 AlexisVon
// SPDX-License-Identifier: MIT

// test_node.js — single-file Node.js encode/decode for cross-language testing
//
// Usage:
//   node test_node.js encode <out.bin>   write standard test data to binary file
//   node test_node.js decode <in.bin>    read binary, verify all fields, exit 0 on pass

const alxbase = require("../nodejs/bin/alxbase");
const fs = require("fs");

const TEST_DATA = {
  bool_true: true,
  bool_false: false,
  int_small: 42,
  int_neg: -128,
  int_zero: 0,
  int_large: BigInt(123456789012345),
  float_val: 3.141592653589793,
  string: "hello alxbase",
  string_empty: "",
  unicode: "你好世界",
  null_val: null,
  bytes_val: new Uint8Array([0, 1, 2, 255, 128]).buffer,
  list_mixed: [1, "two", true, null, 3.14],
  list_nested: [
    [1, 2],
    [3, 4],
  ],
  nested_map: { a: 1, b: { c: "deep" } },
};

function deepEqual(a, b, path = "") {
  if (a === b) return true;

  // null check
  if (a === null || b === null) return a === b;

  // ArrayBuffer vs Uint8Array
  if (a instanceof ArrayBuffer) a = new Uint8Array(a);
  if (b instanceof ArrayBuffer) b = new Uint8Array(b);

  const ta = Object.prototype.toString.call(a);
  const tb = Object.prototype.toString.call(b);

  // TypedArray comparison
  if (ArrayBuffer.isView(a) && ArrayBuffer.isView(b)) {
    if (a.constructor !== b.constructor) {
      // Uint8ClampedArray → Uint8Array is OK
      if (!(a instanceof Uint8Array && b instanceof Uint8Array)) {
        console.error(`TYPE MISMATCH at ${path}: ${a.constructor.name} vs ${b.constructor.name}`);
        return false;
      }
    }
    if (a.length !== b.length) {
      console.error(`LENGTH MISMATCH at ${path}: ${a.length} vs ${b.length}`);
      return false;
    }
    for (let i = 0; i < a.length; i++) {
      if (a[i] !== b[i] && !(Number.isNaN(a[i]) && Number.isNaN(b[i]))) {
        console.error(`ELEM MISMATCH at ${path}[${i}]: ${a[i]} vs ${b[i]}`);
        return false;
      }
    }
    return true;
  }

  // Array
  if (Array.isArray(a) && Array.isArray(b)) {
    if (a.length !== b.length) {
      console.error(`ARRAY LENGTH at ${path}: ${a.length} vs ${b.length}`);
      return false;
    }
    for (let i = 0; i < a.length; i++) {
      if (!deepEqual(a[i], b[i], `${path}[${i}]`)) return false;
    }
    return true;
  }

  // Object
  if (ta === "[object Object]" && tb === "[object Object]") {
    const keysA = Object.keys(a).sort();
    const keysB = Object.keys(b).sort();
    if (keysA.length !== keysB.length) {
      console.error(`KEY COUNT at ${path}: ${keysA.length} vs ${keysB.length}`);
      return false;
    }
    for (const k of keysA) {
      if (!(k in b)) {
        console.error(`MISSING KEY at ${path}.${k}`);
        return false;
      }
      if (!deepEqual(a[k], b[k], `${path}.${k}`)) return false;
    }
    return true;
  }

  // BigInt
  if (typeof a === "bigint" && typeof b === "bigint") return a === b;
  if (typeof a === "bigint" && typeof b === "number") return Number(a) === b;
  if (typeof a === "number" && typeof b === "bigint") return a === Number(b);

  console.error(`MISMATCH at ${path}: ${typeof a}(${a}) vs ${typeof b}(${b})`);
  return false;
}

const cmd = process.argv[2];
const file = process.argv[3];

if (cmd === "encode") {
  const buf = alxbase.encode(TEST_DATA);
  fs.writeFileSync(file, Buffer.from(buf));
  console.log(`encoded ${buf.byteLength} bytes → ${file}`);
} else if (cmd === "decode") {
  const raw = fs.readFileSync(file);
  const ab = new Uint8Array(raw).buffer;
  const obj = alxbase.decode(ab);

  let fail = 0;
  for (const [key, expected] of Object.entries(TEST_DATA)) {
    if (!(key in obj)) {
      console.error(`MISSING: ${key}`);
      fail++;
    } else if (!deepEqual(obj[key], expected, key)) {
      fail++;
    }
  }
  if (fail) {
    console.error(`${fail} FIELD(S) FAILED`);
    process.exit(1);
  }
  console.log(`decode OK: ${file} (${raw.length} bytes)`);
} else {
  console.error("Usage: node test_node.js encode|decode <file>");
  process.exit(1);
}
