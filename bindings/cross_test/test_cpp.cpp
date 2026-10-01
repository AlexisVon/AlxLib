// Copyright (c) 2026 AlexisVon
// SPDX-License-Identifier: MIT

#include "avariant.h"
#include "avarsolid.h"
#include <fstream>
#include <iostream>

using namespace alx;

struct test_case {
    const char* key;
    variant value;
};

static const test_case TEST_DATA[] = {
    {"bool_true", true},
    {"bool_false", false},
    {"int_small", 42},
    {"int_neg", -128},
    {"int_zero", 0},
    {"int_large", (int_64) 123456789012345LL},
    {"float_val", 3.141592653589793},
    {"string", "hello alxbase"},
    {"string_empty", ""},
    {"unicode", "\xe4\xbd\xa0\xe5\xa5\xbd\xe4\xb8\x96\xe7\x95\x8c"},
    {"null_val", variant()},
    {"bytes_val", [] { uint_8 raw[] = {0,1,2,255,128}; return bytes(raw, 5); }()},
    {"list_mixed", varvec{1, std::string("two"), true, variant(), 3.14}},
    {"list_nested", varvec{varvec{1, 2}, varvec{3, 4}}},
    {"nested_map", varmap{{"a", 1}, {"b", varmap{{"c", std::string("deep")}}}}},
};
static const int TEST_COUNT = sizeof(TEST_DATA) / sizeof(TEST_DATA[0]);

static int do_encode(const char* _path) {
    varmap data;
    for (int i = 0; i < TEST_COUNT; i++)
        data[TEST_DATA[i].key] = TEST_DATA[i].value;

    bytes buf = varsolid::to_bytes(data);
    if (buf.empty()) {
        std::cerr << "ENCODE FAIL" << std::endl;
        return 1;
    }

    std::ofstream out(_path, std::ios::binary);
    out.write((const char*) buf.data(), buf.size());
    std::cout << "encoded " << buf.size() << " bytes → " << _path << std::endl;
    return out.good() ? 0 : 1;
}

static bool variant_eq(const variant& _a, const variant& _b) {
    if (_a.type() != _b.type()) {

        if (_a.is_integer() && _b.is_integer())
            return _a.to<int_64>() == _b.to<int_64>();
        return false;
    }
    switch (_a.type()) {
    case variant::id<bool>(): return _a.to<bool>() == _b.to<bool>();
    case variant::id<int_32>(): return _a.to<int_32>() == _b.to<int_32>();
    case variant::id<int_64>(): return _a.to<int_64>() == _b.to<int_64>();
    case variant::id<uint_32>(): return _a.to<uint_32>() == _b.to<uint_32>();
    case variant::id<uint_64>(): return _a.to<uint_64>() == _b.to<uint_64>();
    case variant::id<real_64>(): return _a.to<real_64>() == _b.to<real_64>();
    case variant::id<std::string>(): return _a.to<std::string>() == _b.to<std::string>();
    case variant::id<bytes>(): return _a.to<bytes>() == _b.to<bytes>();
    case variant::id<varmap>(): {
        const varmap& ma = _a.to<varmap>();
        const varmap& mb = _b.to<varmap>();
        if (ma.size() != mb.size()) return false;
        for (const auto& kv_a : ma) {
            bool found = false;
            for (const auto& kv_b : mb) {
                if (kv_a.first == kv_b.first) {
                    if (!variant_eq(*kv_a.second, *kv_b.second)) return false;
                    found = true;
                    break;
                }
            }
            if (!found) return false;
        }
        return true;
    }
    case variant::id<varvec>(): {
        const varvec& va = _a.to<varvec>();
        const varvec& vb = _b.to<varvec>();
        if (va.size() != vb.size()) return false;
        for (size_t i = 0; i < va.size(); i++)
            if (!variant_eq(va[i], vb[i])) return false;
        return true;
    }
    default: return true;
    }
}

static int do_decode(const char* _path) {
    std::ifstream in(_path, std::ios::binary | std::ios::ate);
    if (!in) {
        std::cerr << "CANNOT OPEN " << _path << std::endl;
        return 1;
    }
    size_t size = in.tellg();
    in.seekg(0);
    bytes buf;
    buf.resize(size);
    in.read((char*) buf.data(), size);

    varmap result;
    if (!varsolid::to_varmap(buf, result)) {
        std::cerr << "DECODE FAIL (invalid binary)" << std::endl;
        return 1;
    }

    int fail = 0;
    for (int i = 0; i < TEST_COUNT; i++) {
        bool found = false;
        for (const auto& kv : result) {
            if (kv.first == TEST_DATA[i].key) {
                found = true;
                if (!variant_eq(*kv.second, TEST_DATA[i].value)) {
                    std::cerr << "MISMATCH: " << TEST_DATA[i].key
                              << " (type " << kv.second->type() << ")" << std::endl;
                    fail++;
                }
                break;
            }
        }
        if (!found) {
            std::cerr << "MISSING: " << TEST_DATA[i].key << std::endl;
            fail++;
        }
    }

    if (fail) {
        std::cerr << fail << " FIELD(S) FAILED" << std::endl;
        return 1;
    }
    std::cout << "decode OK: " << _path << " (" << size << " bytes)" << std::endl;
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: test_cpp encode|decode <file>" << std::endl;
        return 1;
    }
    std::string cmd = argv[1];
    if (cmd == "encode") return do_encode(argv[2]);
    if (cmd == "decode") return do_decode(argv[2]);
    std::cerr << "Unknown command: " << cmd << std::endl;
    return 1;
}
