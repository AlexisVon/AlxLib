/*****************************************************************/ /**
 * \file   gt_averify.cpp
 * \brief  Unit tests for verify — abstract hash/CRC interface
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "averify.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <gtest/gtest.h>
#include <iomanip>
#include <memory>
#include <sstream>

using namespace alx;

struct known_answer {
    verify::COMMON_TYPE type;
    const char* input;
    uint_64 len;
    const char* expected;
};

static const known_answer KNOWN_ANSWERS[] = {

    {verify::CRC_32, "123456789", 9, "cbf43926"},
    {verify::CRC_32C, "123456789", 9, "e3069283"},

    {verify::SHA_1, "", 0, "da39a3ee5e6b4b0d3255bfef95601890afd80709"},
    {verify::SHA_1, "abc", 3, "a9993e364706816aba3e25717850c26c9cd0d89d"},

    {verify::SHA_1, "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56,
     "84983e441c3bd26ebaae4aa1f95129e5e54670f1"},

    {verify::SHA_256, "", 0, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
    {verify::SHA_256, "abc", 3, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"},

    {verify::SHA_256, "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56,
     "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"},
};

struct named_answer {
    const char* name;
    const char* input;
    uint_64 len;
    const char* expected;
    uint_8 word;
    bool sha;
};

static const named_answer NAMED_ANSWERS[] = {
    {"crc_8", "123456789", 9, "f4", 4, false},
    {"crc_8_itu", "123456789", 9, "a1", 4, false},
    {"crc_8_rohc", "123456789", 9, "d0", 4, false},
    {"crc_8_maxim", "123456789", 9, "a1", 4, false},
    {"crc_16_ibm", "123456789", 9, "bb3d", 4, false},
    {"crc_16_maxim", "123456789", 9, "44c2", 4, false},
    {"crc_16_usb", "123456789", 9, "b4c8", 4, false},
    {"crc_16_modbus", "123456789", 9, "4b37", 4, false},
    {"crc_16_ccitt", "123456789", 9, "2189", 4, false},
    {"crc_16_ccitt_false", "123456789", 9, "29b1", 4, false},
    {"crc_16_x25", "123456789", 9, "906e", 4, false},
    {"crc_16_xmode", "123456789", 9, "31c3", 4, false},
    {"crc_16_xmode2", "123456789", 9, "0c73", 4, false},
    {"crc_16_dnp", "123456789", 9, "ea82", 4, false},
    {"crc_32", "123456789", 9, "cbf43926", 4, false},
    {"crc_32_c", "123456789", 9, "e3069283", 4, false},
    {"crc_32_koopman", "123456789", 9, "2d3dd0ae", 4, false},
    {"crc_32_mpeg_2", "123456789", 9, "0376e6e7", 4, false},
    {"crc_64_iso", "123456789", 9, "b90956c775a41001", 8, false},
    {"crc_64_ecma", "123456789", 9, "995dc9bbdf1939fa", 8, false},
    {"sha_1", "abc", 3, "a9993e364706816aba3e25717850c26c9cd0d89d", 4, true},
    {"sha_256", "abc", 3, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", 4, true},
};

TEST(gt_averify, exec_by_enum) {
    for (const auto& ka : KNOWN_ANSWERS) {
        auto result = verify::exec(ka.type, (const uint_8*) ka.input, ka.len);
        EXPECT_EQ(result, ka.expected)
            << "exec(enum) mismatch for type=" << (int) ka.type;
    }
}

TEST(gt_averify, exec_by_name) {
    for (const auto& na : NAMED_ANSWERS) {
        auto result = verify::exec(na.name, (const uint_8*) na.input, na.len);
        EXPECT_EQ(result, na.expected)
            << "exec(name) mismatch for " << na.name;
    }
}

TEST(gt_averify, bytedigest_matches_hexdigest_word_order) {
    for (const auto& na : NAMED_ANSWERS) {
        std::unique_ptr<verify> v(verify::create(na.name));
        ASSERT_NE(nullptr, v) << "no product for " << na.name;
        v->update((const uint_8*) na.input, na.len);
        bytes bin = v->bytedigest();

        ASSERT_EQ(0U, bin.size() % na.word) << "digest is not a whole number of words for " << na.name;

        std::string want;
        char byte[3];
        if (na.sha) {
            const bytes standard = bytes::from_hex(na.expected);
            for (uint_64 i = 0; i < standard.size(); i += na.word)
                for (uint_64 k = 0; k < na.word; k++) {
                    snprintf(byte, sizeof(byte), "%02x", standard.data()[i + na.word - 1 - k]);
                    want += byte;
                }
        } else {
            const uint_64 value = strtoull(na.expected, nullptr, 16);
            for (uint_8 k = 0; k < na.word; k++) {
                snprintf(byte, sizeof(byte), "%02x", (unsigned) ((value >> (8 * k)) & 0xFF));
                want += byte;
            }
        }

        std::string got;
        for (uint_64 i = 0; i < bin.size(); i++) {
            snprintf(byte, sizeof(byte), "%02x", bin.data()[i]);
            got += byte;
        }
        EXPECT_EQ(got, want) << "bytedigest/hexdigest disagree for " << na.name;
    }
}

TEST(gt_averify, exec_overloads) {

    bytes b("abc");
    EXPECT_EQ(verify::exec(verify::SHA_1, b),
              "a9993e364706816aba3e25717850c26c9cd0d89d");

    EXPECT_EQ(verify::exec(verify::SHA_1, std::string("abc")),
              "a9993e364706816aba3e25717850c26c9cd0d89d");

    EXPECT_EQ(verify::exec("sha_256", std::string("abc")),
              "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(gt_averify, instance_lifecycle) {
    for (const auto& ka : KNOWN_ANSWERS) {
        verify* v = verify::create(ka.type);
        ASSERT_NE(v, nullptr) << "create failed for type=" << (int) ka.type;

        v->update((const uint_8*) ka.input, ka.len);
        EXPECT_EQ(v->hexdigest(), ka.expected);

        v->clear();
        v->update((const uint_8*) "abc", 3);
        EXPECT_FALSE(v->hexdigest().empty());

        delete v;
    }
}

TEST(gt_averify, incremental_equals_oneshot) {

    for (const auto& ka : KNOWN_ANSWERS) {
        if (ka.len < 2) continue;

        verify* v = verify::create(ka.type);
        ASSERT_NE(v, nullptr);

        for (uint_64 i = 0; i < ka.len; ++i)
            v->update((const uint_8*) &ka.input[i], 1);

        EXPECT_EQ(v->hexdigest(), ka.expected)
            << "incremental mismatch for type=" << (int) ka.type;

        delete v;
    }
}

TEST(gt_averify, bytedigest_nonempty) {
    for (const auto& ka : KNOWN_ANSWERS) {
        verify* v = verify::create(ka.type);
        ASSERT_NE(v, nullptr);
        v->update((const uint_8*) ka.input, ka.len);

        bytes b = v->bytedigest();
        EXPECT_GT(b.size(), 0U) << "bytedigest empty for type=" << (int) ka.type;

        delete v;
    }
}

TEST(gt_averify, hexdigest_and_bytedigest_deterministic) {
    for (const auto& ka : KNOWN_ANSWERS) {

        verify* va = verify::create(ka.type);
        ASSERT_NE(va, nullptr);
        va->update((const uint_8*) ka.input, ka.len);
        std::string hex = va->hexdigest();
        EXPECT_EQ(hex, ka.expected);

        verify* vb = verify::create(ka.type);
        ASSERT_NE(vb, nullptr);
        vb->update((const uint_8*) ka.input, ka.len);
        bytes bin = vb->bytedigest();
        EXPECT_GT(bin.size(), 0U);

        verify* vc = verify::create(ka.type);
        ASSERT_NE(vc, nullptr);
        vc->update((const uint_8*) ka.input, ka.len);
        EXPECT_EQ(vc->hexdigest(), hex);

        verify* vd = verify::create(ka.type);
        ASSERT_NE(vd, nullptr);
        vd->update((const uint_8*) ka.input, ka.len);
        EXPECT_EQ(vd->bytedigest(), bin);

        delete va;
        delete vb;
        delete vc;
        delete vd;
    }
}

TEST(gt_averify, hardcal_returns_bool) {
    for (const auto& ka : KNOWN_ANSWERS) {
        verify* v = verify::create(ka.type);
        ASSERT_NE(v, nullptr);
        bool h = v->hardcal();
        EXPECT_TRUE(h == true || h == false);
        delete v;
    }
}

TEST(gt_averify, list_and_create_all) {
    auto names = verify::list();
    EXPECT_GE(names.size(), 20U);

    for (const auto& name : names) {
        verify* v = verify::create(name);
        ASSERT_NE(v, nullptr) << "factory create failed for: " << name;

        v->update((const uint_8*) "123456789", 9);
        EXPECT_FALSE(v->hexdigest().empty()) << "empty hexdigest for: " << name;
        EXPECT_FALSE(v->bytedigest().empty()) << "empty bytedigest for: " << name;

        delete v;
    }
}

TEST(gt_averify, empty_input) {
    for (auto t : {verify::CRC_32, verify::CRC_32C, verify::SHA_1, verify::SHA_256}) {
        verify* v = verify::create(t);
        ASSERT_NE(v, nullptr);

        v->update((const uint_8*) "", 0);
        EXPECT_FALSE(v->hexdigest().empty());
        EXPECT_FALSE(v->bytedigest().empty());

        delete v;

        EXPECT_FALSE(verify::exec(t, (const uint_8*) "", 0).empty());
    }
}

TEST(gt_averify, large_input) {

    {
        std::string msg(2048, 'a');
        auto r = verify::exec(verify::SHA_1, (const uint_8*) msg.data(), msg.size());
        EXPECT_EQ(r, "dc28591e574d1eac79ad07b7a4d01d5026a02428");
    }

    {
        std::string msg(128, 'a');
        auto r = verify::exec(verify::SHA_256, (const uint_8*) msg.data(), msg.size());
        EXPECT_EQ(r, "6836cf13bac400e9105071cd6af47084dfacad4e5e302c94bfed24e013afb73e");
    }

    {
        std::string msg(1000, 'x');
        auto r = verify::exec(verify::CRC_32, (const uint_8*) msg.data(), msg.size());
        EXPECT_FALSE(r.empty());
    }
}

// HMAC (the keyed part of averify.h); RFC 4231 sections 4.2 to 4.8 are the public vectors
static std::string hmac_hex(const std::string& _key, const std::string& _msg) {
    std::unique_ptr<hmac> mac(hmac::create(hmac::SHA_256));
    mac->init(_key);
    mac->update(_msg);
    return mac->hexdigest();
}

TEST(gt_averify, hmac_rfc4231) {
    EXPECT_EQ(hmac_hex(std::string(20, '\x0b'), "Hi There"), "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");
    EXPECT_EQ(hmac_hex("Jefe", "what do ya want for nothing?"), "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
    EXPECT_EQ(hmac_hex(std::string(20, '\xaa'), std::string(50, '\xdd')), "773ea91e36800e46854db8ebd09181a72959098b3ef8c122d9635514ced565fe");
    EXPECT_EQ(hmac_hex(std::string("\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19", 25), std::string(50, '\xcd')),
              "82558a389a443c0ea4cc819899f2083a85f0faa3e578f8077a2e3ff46729665b");
    EXPECT_EQ(hmac_hex(std::string(20, '\x0c'), "Test With Truncation"), "a3b6167473100ee06e0c796c2955552bfa6f7c0a6a8aef8b93f860aab0cd20c5");
    EXPECT_EQ(hmac_hex(std::string(131, '\xaa'), "Test Using Larger Than Block-Size Key - Hash Key First"),
              "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54");
    EXPECT_EQ(hmac_hex(std::string(131, '\xaa'),
                       "This is a test using a larger than block-size key and a larger than block-size data. The key needs to be hashed before being used by the HMAC "
                       "algorithm."),
              "9b09ffa71b942fcb27635fbcd5b0e944bfdc63644f0713938a7f51535c3a35e2");
}

TEST(gt_averify, hmac_empty_key_and_message) {
    EXPECT_EQ(hmac_hex("", ""), "b613679a0814d9ec772f95d778c35fc5ff1697c493715653c6c712144292c5ad");
    EXPECT_EQ(hmac_hex("key", ""), "5d5d139563c95b5967b9bd9a8c9b233a9dedb45072794cd232dc1b74832607d0");
}

TEST(gt_averify, hmac_incremental_equals_oneshot) {
    const std::string msg = "the quick brown fox jumps over the lazy dog, twice over";
    const std::string one_shot = hmac_hex("key", msg);

    std::unique_ptr<hmac> mac(hmac::create(hmac::SHA_256));
    mac->init("key");
    for (size_t pos = 0; pos < msg.size(); pos += 7) mac->update(msg.substr(pos, 7));
    EXPECT_EQ(mac->hexdigest(), one_shot);
}

TEST(gt_averify, hmac_key_survives_clear) {
    std::unique_ptr<hmac> mac(hmac::create(hmac::SHA_256));
    mac->init("key");
    mac->update("first message");
    EXPECT_EQ(mac->hexdigest(), hmac_hex("key", "first message"));

    mac->clear();
    mac->update("second message");
    EXPECT_EQ(mac->hexdigest(), hmac_hex("key", "second message"));

    mac->init("another key");
    mac->update("first message");
    EXPECT_EQ(mac->hexdigest(), hmac_hex("another key", "first message"));
}

// bytedigest() is verify's word dump: the same MAC as hexdigest(), words in machine order
TEST(gt_averify, hmac_bytedigest_is_word_dump) {
    std::unique_ptr<hmac> mac(hmac::create(hmac::SHA_256));
    mac->init("key");
    mac->update("abc");
    bytes raw = mac->bytedigest();
    ASSERT_EQ(raw.size(), 32U);

    std::stringstream ss;
    for (int i = 0; i < 8; ++i) {
        uint_32 word;
        memcpy(&word, raw.data() + i * sizeof(word), sizeof(word));
        ss << std::hex << std::setfill('0') << std::setw(8) << word;
    }
    EXPECT_EQ(ss.str(), hmac_hex("key", "abc"));
}

TEST(gt_averify, hmac_factory_and_models) {
    const std::vector<std::string> names = hmac::list();
    EXPECT_NE(std::find(names.begin(), names.end(), "hmac_sha_256"), names.end());

    std::unique_ptr<hmac> by_name(hmac::create("hmac_sha_256"));
    ASSERT_NE(by_name, nullptr);
    by_name->init("Jefe");
    by_name->update("what do ya want for nothing?");
    EXPECT_EQ(by_name->hexdigest(), "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");

    EXPECT_EQ(hmac::create(hmac::NO_CHECK), nullptr);
    EXPECT_EQ(hmac::create("sha_256"), nullptr);
}
