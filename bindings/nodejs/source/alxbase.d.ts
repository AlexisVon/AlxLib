// Copyright (c) 2026 AlexisVon
// SPDX-License-Identifier: MIT

// TypeScript declarations for alxvar native addon
// Note: the .node binary exports are available via require("../bin/alxvar")

/** Return library version string, e.g. "v1.0.0, build: 12:00:00 Jun 22 2026" */
export function version(): string;

/**
 * Encode a plain JS object to binary (ArrayBuffer).
 * Supported value types: boolean, number (int32/uint32/float64),
 * bigint (int64/uint64), string, null, ArrayBuffer,
 * TypedArray (Int8Array .. BigUint64Array), plain Object (recursive),
 * Array (recursive).
 */
export function encode(obj: Record<string, any>): ArrayBuffer;

/**
 * Decode binary (Uint8Array) to a plain JS object.
 * @param buf - Uint8Array containing binary data encoded by encode()
 */
export function decode(buf: Uint8Array): Record<string, any>;

/**
 * Decode and extract a value by path from binary (Uint8Array).
 * @param buf - Uint8Array containing binary data encoded by encode()
 * @param path - one or more string keys (or numeric string for array indices)
 * @returns the value at the path, or undefined if not found
 */
export function decode(buf: Uint8Array, ...path: string[]): any;
