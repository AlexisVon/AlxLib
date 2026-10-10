/*****************************************************************/ /**
 * \file   gt_aaes.cpp
 * \brief  Unit tests for AES (ECB, CBC, CTR, GCM)
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "aaes.h"
#include <cstring>
#include <gtest/gtest.h>

#if defined(_MSC_VER)
#    include <intrin.h>
#endif

using namespace alx;

template <uint_64 N>
static inline void from_hex(const char* _hex, uint_8 (&_out)[N]) {
    bytes b = bytes::from_hex(_hex);
    memcpy(_out, b.data(), N);
}

TEST(gt_aaes, ecb_128_encrypt) {
    uint_8 key[16];
    from_hex("2b7e151628aed2a6abf7158809cf4f3c", key);

    bytes pt = bytes::from_hex("6bc1bee22e409f96e93d7e117393172a");
    bytes expected = bytes::from_hex("3ad77bb40d7a3660a89ecaf32466ef97");

    aes_ecb<128> ecb(key);
    ASSERT_TRUE(ecb.encrypt(pt));
    EXPECT_EQ(pt, expected);
}

TEST(gt_aaes, ecb_128_decrypt) {
    uint_8 key[16];
    from_hex("2b7e151628aed2a6abf7158809cf4f3c", key);

    bytes ct = bytes::from_hex("3ad77bb40d7a3660a89ecaf32466ef97");
    bytes expected = bytes::from_hex("6bc1bee22e409f96e93d7e117393172a");

    aes_ecb<128> ecb(key);
    ASSERT_TRUE(ecb.decrypt(ct));
    EXPECT_EQ(ct, expected);
}

TEST(gt_aaes, ecb_128_roundtrip) {
    uint_8 key[16];
    from_hex("2b7e151628aed2a6abf7158809cf4f3c", key);

    bytes orig = bytes::from_hex("6bc1bee22e409f96e93d7e117393172a");
    bytes buf(orig);

    aes_ecb<128> ecb(key);
    ASSERT_TRUE(ecb.encrypt(buf));
    EXPECT_NE(buf, orig);
    ASSERT_TRUE(ecb.decrypt(buf));
    EXPECT_EQ(buf, orig);
}

TEST(gt_aaes, ecb_128_multi_block) {
    uint_8 key[16];
    from_hex("2b7e151628aed2a6abf7158809cf4f3c", key);

    bytes pt = bytes::from_hex(
        "6bc1bee22e409f96e93d7e117393172a"
        "ae2d8a571e03ac9c9eb76fac45af8e51");
    bytes expected = bytes::from_hex(
        "3ad77bb40d7a3660a89ecaf32466ef97"
        "f5d3d58503b9699de785895a96fdbaaf");

    aes_ecb<128> ecb(key);
    ASSERT_TRUE(ecb.encrypt(pt));
    EXPECT_EQ(pt, expected);
}

TEST(gt_aaes, ecb_256_encrypt) {
    uint_8 key[32];
    from_hex("603deb1015ca71be2b73aef0857d7781"
             "1f352c073b6108d72d9810a30914dff4",
             key);

    bytes pt = bytes::from_hex("6bc1bee22e409f96e93d7e117393172a");
    bytes expected = bytes::from_hex("f3eed1bdb5d2a03c064b5a7e3db181f8");

    aes_ecb<256> ecb(key);
    ASSERT_TRUE(ecb.encrypt(pt));
    EXPECT_EQ(pt, expected);
}

TEST(gt_aaes, ecb_256_roundtrip) {
    uint_8 key[32];
    from_hex("603deb1015ca71be2b73aef0857d7781"
             "1f352c073b6108d72d9810a30914dff4",
             key);

    bytes orig = bytes::from_hex("6bc1bee22e409f96e93d7e117393172a");
    bytes buf(orig);

    aes_ecb<256> ecb(key);
    ASSERT_TRUE(ecb.encrypt(buf));
    ASSERT_TRUE(ecb.decrypt(buf));
    EXPECT_EQ(buf, orig);
}

TEST(gt_aaes, cbc_128_encrypt) {
    uint_8 key[16], iv[16];
    from_hex("2b7e151628aed2a6abf7158809cf4f3c", key);
    from_hex("000102030405060708090a0b0c0d0e0f", iv);

    bytes pt = bytes::from_hex(
        "6bc1bee22e409f96e93d7e117393172a"
        "ae2d8a571e03ac9c9eb76fac45af8e51"
        "30c81c46a35ce411e5fbc1191a0a52ef"
        "f69f2445df4f9b17ad2b417be66c3710");
    bytes expected = bytes::from_hex(
        "7649abac8119b246cee98e9b12e9197d"
        "5086cb9b507219ee95db113a917678b2"
        "73bed6b8e3c1743b7116e69e22229516"
        "3ff1caa1681fac09120eca307586e1a7");

    aes_cbc<128> cbc(key, iv);
    ASSERT_TRUE(cbc.encrypt(pt));
    EXPECT_EQ(pt, expected);
}

TEST(gt_aaes, cbc_128_decrypt) {
    uint_8 key[16], iv[16];
    from_hex("2b7e151628aed2a6abf7158809cf4f3c", key);
    from_hex("000102030405060708090a0b0c0d0e0f", iv);

    bytes ct = bytes::from_hex(
        "7649abac8119b246cee98e9b12e9197d"
        "5086cb9b507219ee95db113a917678b2"
        "73bed6b8e3c1743b7116e69e22229516"
        "3ff1caa1681fac09120eca307586e1a7");
    bytes expected = bytes::from_hex(
        "6bc1bee22e409f96e93d7e117393172a"
        "ae2d8a571e03ac9c9eb76fac45af8e51"
        "30c81c46a35ce411e5fbc1191a0a52ef"
        "f69f2445df4f9b17ad2b417be66c3710");

    aes_cbc<128> cbc(key, iv);
    ASSERT_TRUE(cbc.decrypt(ct));
    EXPECT_EQ(ct, expected);
}

TEST(gt_aaes, cbc_128_roundtrip) {
    uint_8 key[16], iv[16];
    from_hex("2b7e151628aed2a6abf7158809cf4f3c", key);
    from_hex("000102030405060708090a0b0c0d0e0f", iv);

    bytes orig = bytes::from_hex(
        "6bc1bee22e409f96e93d7e117393172a"
        "ae2d8a571e03ac9c9eb76fac45af8e51");
    bytes buf(orig);

    aes_cbc<128> cbc(key, iv);
    ASSERT_TRUE(cbc.encrypt(buf));
    EXPECT_NE(buf, orig);
    cbc.reset();
    ASSERT_TRUE(cbc.decrypt(buf));
    EXPECT_EQ(buf, orig);
}

TEST(gt_aaes, cbc_256_roundtrip) {
    uint_8 key[32], iv[16];
    from_hex("603deb1015ca71be2b73aef0857d7781"
             "1f352c073b6108d72d9810a30914dff4",
             key);
    from_hex("000102030405060708090a0b0c0d0e0f", iv);

    bytes orig = bytes::from_hex(
        "6bc1bee22e409f96e93d7e117393172a"
        "ae2d8a571e03ac9c9eb76fac45af8e51");
    bytes buf(orig);

    aes_cbc<256> cbc(key, iv);
    ASSERT_TRUE(cbc.encrypt(buf));
    EXPECT_NE(buf, orig);
    cbc.reset();
    ASSERT_TRUE(cbc.decrypt(buf));
    EXPECT_EQ(buf, orig);
}

TEST(gt_aaes, ctr_128_xcrypt) {
    uint_8 key[16], iv[16];
    from_hex("2b7e151628aed2a6abf7158809cf4f3c", key);
    from_hex("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff", iv);

    bytes orig = bytes::from_hex(
        "6bc1bee22e409f96e93d7e117393172a"
        "ae2d8a571e03ac9c9eb76fac45af8e51"
        "30c81c46a35ce411e5fbc1191a0a52ef");
    bytes buf(orig);

    aes_ctr<128> ctr(key, iv);
    ASSERT_TRUE(ctr.encrypt(buf));
    EXPECT_NE(buf, orig);
    ctr.reset();
    ASSERT_TRUE(ctr.decrypt(buf));
    EXPECT_EQ(buf, orig);
}

TEST(gt_aaes, ctr_128_unaligned) {
    uint_8 key[16], iv[16];
    memset(key, 0x2b, 16);
    memset(iv, 0xf0, 16);

    bytes orig(21, 0x6b);
    bytes buf(orig);

    aes_ctr<128> ctr(key, iv);
    ASSERT_TRUE(ctr.encrypt(buf));
    EXPECT_NE(buf, orig);
    ctr.reset();
    ASSERT_TRUE(ctr.decrypt(buf));
    EXPECT_EQ(buf, orig);
}

TEST(gt_aaes, ctr_256_roundtrip) {
    uint_8 key[32], iv[16];
    from_hex("603deb1015ca71be2b73aef0857d7781"
             "1f352c073b6108d72d9810a30914dff4",
             key);
    from_hex("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff", iv);

    bytes orig(64, 0x6b);
    bytes buf(orig);

    aes_ctr<256> ctr(key, iv);
    ASSERT_TRUE(ctr.encrypt(buf));
    EXPECT_NE(buf, orig);
    ctr.reset();
    ASSERT_TRUE(ctr.decrypt(buf));
    EXPECT_EQ(buf, orig);
}

TEST(gt_aaes, gcm_128_empty_pt_empty_aad) {
    uint_8 key[16], iv[12];
    memset(key, 0, 16);
    memset(iv, 0, 12);

    aes_gcm<128> gcm(key, iv);
    ASSERT_TRUE(gcm.start(nullptr, 0));
    ASSERT_TRUE(gcm.encrypt(nullptr, 0));

    uint_8 tag[16];
    ASSERT_TRUE(gcm.finish(tag));

    bytes expected = bytes::from_hex("58e2fccefa7e3061367f1d57a4e7455a");
    EXPECT_EQ(memcmp(tag, expected.data(), 16), 0);
}

TEST(gt_aaes, gcm_128_data_no_aad) {
    uint_8 key[16], iv[12];
    memset(key, 0, 16);
    memset(iv, 0, 12);

    bytes pt = bytes::from_hex("00000000000000000000000000000000");
    bytes expected = bytes::from_hex("0388dace60b6a392f328c2b971b2fe78");

    aes_gcm<128> gcm(key, iv);
    ASSERT_TRUE(gcm.start(nullptr, 0));
    ASSERT_TRUE(gcm.encrypt(pt.data(), pt.size()));
    EXPECT_EQ(pt, expected);

    uint_8 tag[16];
    ASSERT_TRUE(gcm.finish(tag));

    bytes expected_tag = bytes::from_hex("ab6e47d42cec13bdf53a67b21257bddf");
    EXPECT_EQ(memcmp(tag, expected_tag.data(), 16), 0);
}

TEST(gt_aaes, gcm_128_data_with_aad) {
    uint_8 key[16], iv[12];
    memset(key, 0, 16);
    memset(iv, 0, 12);

    bytes aad = bytes::from_hex("00000000000000000000000000000000");
    bytes pt = bytes::from_hex("00000000000000000000000000000000");
    bytes expected_ct = bytes::from_hex("0388dace60b6a392f328c2b971b2fe78");

    aes_gcm<128> gcm(key, iv);
    ASSERT_TRUE(gcm.start(aad));
    ASSERT_TRUE(gcm.encrypt(pt.data(), pt.size()));
    EXPECT_EQ(pt, expected_ct);

    uint_8 tag[16];
    ASSERT_TRUE(gcm.finish(tag));

    bytes no_aad_tag = bytes::from_hex("ab6e47d42cec13bdf53a67b21257bddf");
    EXPECT_NE(memcmp(tag, no_aad_tag.data(), 16), 0);
}

TEST(gt_aaes, gcm_128_roundtrip) {
    uint_8 key[16], iv[12];
    from_hex("feffe9928665731c6d6a8f9467308308", key);
    from_hex("cafebabefacedbaddecaf888", iv);

    bytes aad = bytes::from_hex("feedfacedeadbeeffeedfacedeadbeef");
    bytes orig(128, 0x42);
    bytes buf(orig);

    aes_gcm<128> gcm_enc(key, iv);
    ASSERT_TRUE(gcm_enc.start(aad));
    ASSERT_TRUE(gcm_enc.encrypt(buf.data(), buf.size()));
    uint_8 enc_tag[16];
    ASSERT_TRUE(gcm_enc.finish(enc_tag));

    aes_gcm<128> gcm_dec(key, iv);
    ASSERT_TRUE(gcm_dec.start(aad));
    ASSERT_TRUE(gcm_dec.decrypt(buf.data(), buf.size()));
    uint_8 dec_tag[16];
    ASSERT_TRUE(gcm_dec.finish(dec_tag));

    EXPECT_EQ(memcmp(enc_tag, dec_tag, 16), 0);
    EXPECT_EQ(buf, orig);
}

TEST(gt_aaes, gcm_128_wrong_aad_different_tag) {
    uint_8 key[16], iv[12];
    from_hex("feffe9928665731c6d6a8f9467308308", key);
    from_hex("cafebabefacedbaddecaf888", iv);

    bytes aad = bytes::from_hex("feedfacedeadbeeffeedfacedeadbeef");
    bytes wrong_aad = bytes::from_hex("deadbeeffeedfacedeadbeeffeedface");
    bytes orig(32, 0x42);
    bytes buf(orig);

    aes_gcm<128> gcm_enc(key, iv);
    gcm_enc.start(aad);
    gcm_enc.encrypt(buf.data(), buf.size());
    uint_8 enc_tag[16];
    gcm_enc.finish(enc_tag);

    aes_gcm<128> gcm_dec(key, iv);
    gcm_dec.start(wrong_aad);
    gcm_dec.decrypt(buf.data(), buf.size());
    uint_8 dec_tag[16];
    gcm_dec.finish(dec_tag);

    EXPECT_NE(memcmp(enc_tag, dec_tag, 16), 0);
}

TEST(gt_aaes, gcm_128_unaligned) {
    uint_8 key[16], iv[12];
    memset(key, 0, 16);
    memset(iv, 0, 12);

    bytes orig(37, 0x55);
    bytes buf(orig);

    aes_gcm<128> gcm(key, iv);
    ASSERT_TRUE(gcm.start(nullptr, 0));
    ASSERT_TRUE(gcm.encrypt(buf.data(), buf.size()));
    uint_8 tag1[16];
    ASSERT_TRUE(gcm.finish(tag1));

    aes_gcm<128> gcm2(key, iv);
    ASSERT_TRUE(gcm2.start(nullptr, 0));
    ASSERT_TRUE(gcm2.decrypt(buf.data(), buf.size()));
    uint_8 tag2[16];
    ASSERT_TRUE(gcm2.finish(tag2));

    EXPECT_EQ(memcmp(tag1, tag2, 16), 0);
    EXPECT_EQ(buf, orig);
}

// Published vectors: GCM spec (McGrew-Viega), Appendix B, test cases 3 and 4. They cover a
// multi-block message and a partial trailing block with AAD -- shapes the cases above exercise
// too, but only these two carry an expected answer that came from outside this repo
TEST(gt_aaes, gcm_128_public_vectors) {
    uint_8 key[16], iv[12], tag[16];
    from_hex("feffe9928665731c6d6a8f9467308308", key);
    from_hex("cafebabefacedbaddecaf888", iv);

    bytes pt = bytes::from_hex("d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a72"
                               "1c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b391aafd255");
    bytes expected_ct = bytes::from_hex("42831ec2217774244b7221b784d0d49ce3aa212f2c02a4e035c17e2329aca12e"
                                        "21d514b25466931c7d8f6a5aac84aa051ba30b396a0aac973d58e091473f5985");
    bytes expected_tag = bytes::from_hex("4d5c2af327cd64a62cf35abd2ba6fab4");

    aes_gcm<128> gcm(key, iv);
    ASSERT_TRUE(gcm.start(nullptr, 0));
    ASSERT_TRUE(gcm.encrypt(pt.data(), pt.size()));
    EXPECT_EQ(pt, expected_ct);
    ASSERT_TRUE(gcm.finish(tag));
    EXPECT_EQ(memcmp(tag, expected_tag.data(), 16), 0);

    bytes aad = bytes::from_hex("feedfacedeadbeeffeedfacedeadbeefabaddad2");
    bytes short_pt = bytes::from_hex("d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a72"
                                     "1c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b39");
    bytes short_ct = bytes::from_hex("42831ec2217774244b7221b784d0d49ce3aa212f2c02a4e035c17e2329aca12e"
                                     "21d514b25466931c7d8f6a5aac84aa051ba30b396a0aac973d58e091");
    bytes short_tag = bytes::from_hex("5bc94fbc3221a5db94fae95ae7121a47");

    aes_gcm<128> gcm4(key, iv);
    ASSERT_TRUE(gcm4.start(aad));
    ASSERT_TRUE(gcm4.encrypt(short_pt.data(), short_pt.size()));
    EXPECT_EQ(short_pt, short_ct);
    ASSERT_TRUE(gcm4.finish(tag));
    EXPECT_EQ(memcmp(tag, short_tag.data(), 16), 0);
}

// Published vectors: the same appendix, test cases 13 and 14 -- the AES-256 pair of the two above
TEST(gt_aaes, gcm_256_public_vectors) {
    uint_8 key[32], iv[12], tag[16];
    from_hex("feffe9928665731c6d6a8f9467308308"
             "feffe9928665731c6d6a8f9467308308",
             key);
    from_hex("cafebabefacedbaddecaf888", iv);

    bytes pt = bytes::from_hex("d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a72"
                               "1c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b391aafd255");
    bytes expected_ct = bytes::from_hex("522dc1f099567d07f47f37a32a84427d643a8cdcbfe5c0c97598a2bd2555d1aa"
                                        "8cb08e48590dbb3da7b08b1056828838c5f61e6393ba7a0abcc9f662898015ad");
    bytes expected_tag = bytes::from_hex("b094dac5d93471bdec1a502270e3cc6c");

    aes_gcm<256> gcm(key, iv);
    ASSERT_TRUE(gcm.start(nullptr, 0));
    ASSERT_TRUE(gcm.encrypt(pt.data(), pt.size()));
    EXPECT_EQ(pt, expected_ct);
    ASSERT_TRUE(gcm.finish(tag));
    EXPECT_EQ(memcmp(tag, expected_tag.data(), 16), 0);

    bytes aad = bytes::from_hex("feedfacedeadbeeffeedfacedeadbeefabaddad2");
    bytes short_pt = bytes::from_hex("d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a72"
                                     "1c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b39");
    bytes short_ct = bytes::from_hex("522dc1f099567d07f47f37a32a84427d643a8cdcbfe5c0c97598a2bd2555d1aa"
                                     "8cb08e48590dbb3da7b08b1056828838c5f61e6393ba7a0abcc9f662");
    bytes short_tag = bytes::from_hex("76fc6ece0f4e1768cddf8853bb2d551b");

    aes_gcm<256> gcm14(key, iv);
    ASSERT_TRUE(gcm14.start(aad));
    ASSERT_TRUE(gcm14.encrypt(short_pt.data(), short_pt.size()));
    EXPECT_EQ(short_pt, short_ct);
    ASSERT_TRUE(gcm14.finish(tag));
    EXPECT_EQ(memcmp(tag, short_tag.data(), 16), 0);
}

TEST(gt_aaes, gcm_256_roundtrip) {
    uint_8 key[32], iv[12];
    from_hex("603deb1015ca71be2b73aef0857d7781"
             "1f352c073b6108d72d9810a30914dff4",
             key);
    memset(iv, 0xab, 12);

    bytes aad = bytes::from_hex("deadbeefcafebabe");
    bytes orig(256, 0x7a);
    bytes buf(orig);

    aes_gcm<256> gcm_enc(key, iv);
    ASSERT_TRUE(gcm_enc.start(aad));
    ASSERT_TRUE(gcm_enc.encrypt(buf.data(), buf.size()));
    uint_8 enc_tag[16];
    ASSERT_TRUE(gcm_enc.finish(enc_tag));

    aes_gcm<256> gcm_dec(key, iv);
    ASSERT_TRUE(gcm_dec.start(aad));
    ASSERT_TRUE(gcm_dec.decrypt(buf.data(), buf.size()));
    uint_8 dec_tag[16];
    ASSERT_TRUE(gcm_dec.finish(dec_tag));

    EXPECT_EQ(memcmp(enc_tag, dec_tag, 16), 0);
    EXPECT_EQ(buf, orig);
}

TEST(gt_aaes, padding_pkcs7) {

    bytes buf(15, 0x41);
    aes_ecb<128>::buf_padding(buf, aes_base::PADDING::PKCS7);
    EXPECT_EQ(buf.size(), 16U);
    EXPECT_EQ(buf[15], 0x01U);

    aes_ecb<128>::buf_unpadding(buf, aes_base::PADDING::PKCS7);
    EXPECT_EQ(buf.size(), 15U);

    bytes buf2(16, 0x42);
    aes_ecb<128>::buf_padding(buf2, aes_base::PADDING::PKCS7);
    EXPECT_EQ(buf2.size(), 32U);
    EXPECT_EQ(buf2.mid(16), bytes(16, 0x10U));

    aes_ecb<128>::buf_unpadding(buf2, aes_base::PADDING::PKCS7);
    EXPECT_EQ(buf2.size(), 16U);

    bytes buf3;
    aes_ecb<128>::buf_padding(buf3, aes_base::PADDING::PKCS7);
    EXPECT_EQ(buf3.size(), 16U);
    EXPECT_EQ(buf3, bytes(16, 0x10U));
}

TEST(gt_aaes, padding_zeros) {
    bytes buf(7, 0x41);
    aes_ecb<128>::buf_padding(buf, aes_base::PADDING::ZEROS);
    EXPECT_EQ(buf.size(), 16U);
    EXPECT_EQ(buf.mid(7), bytes(9, 0x00U));

    aes_ecb<128>::buf_unpadding(buf, aes_base::PADDING::ZEROS);
    EXPECT_EQ(buf.size(), 7U);
}

TEST(gt_aaes, padding_ansix923) {
    bytes buf(5, 0x41);
    aes_ecb<128>::buf_padding(buf, aes_base::PADDING::ANSIX923);
    EXPECT_EQ(buf.size(), 16U);
    EXPECT_EQ(buf[15], 0X0BU);
    EXPECT_EQ(buf.mid(5, 10), bytes(10, 0X00U));
    aes_ecb<128>::buf_unpadding(buf, aes_base::PADDING::ANSIX923);
    EXPECT_EQ(buf.size(), 5U);

    bytes blk(16, 0x42);
    aes_ecb<128>::buf_padding(blk, aes_base::PADDING::ANSIX923);
    EXPECT_EQ(blk.size(), 32U);
    EXPECT_EQ(blk[31], 0X10U);
    EXPECT_EQ(blk.mid(16, 15), bytes(15, 0X00U));
    aes_ecb<128>::buf_unpadding(blk, aes_base::PADDING::ANSIX923);
    EXPECT_EQ(blk.size(), 16U);
}

TEST(gt_aaes, padding_ansix923_invalid_length) {

    bytes buf(16, 0x41);
    buf[15] = 0X00U;
    aes_ecb<128>::buf_unpadding(buf, aes_base::PADDING::ANSIX923);
    EXPECT_EQ(buf.size(), 16U);
}

TEST(gt_aaes, padding_iso10126) {
    bytes buf(5, 0x41);
    aes_ecb<128>::buf_padding(buf, aes_base::PADDING::ISO10126);
    EXPECT_EQ(buf.size(), 16U);
    EXPECT_EQ(buf[15], 0X0BU);
    EXPECT_EQ(buf.mid(5, 10), bytes(10, 0XFFU));
    aes_ecb<128>::buf_unpadding(buf, aes_base::PADDING::ISO10126);
    EXPECT_EQ(buf.size(), 5U);

    bytes blk(16, 0x42);
    aes_ecb<128>::buf_padding(blk, aes_base::PADDING::ISO10126);
    EXPECT_EQ(blk.size(), 32U);
    EXPECT_EQ(blk[31], 0X10U);
    aes_ecb<128>::buf_unpadding(blk, aes_base::PADDING::ISO10126);
    EXPECT_EQ(blk.size(), 16U);
}

TEST(gt_aaes, ecb_rejects_unaligned) {
    uint_8 key[16]{};
    aes_ecb<128> ecb(key);

    bytes buf(15, 0x41);
    EXPECT_FALSE(ecb.encrypt(buf));

    bytes buf2;
    EXPECT_FALSE(ecb.encrypt(buf2));
}

TEST(gt_aaes, gcm_double_start_rejected) {
    uint_8 key[16]{}, iv[12]{};
    aes_gcm<128> gcm(key, iv);
    ASSERT_TRUE(gcm.start(nullptr, 0));
    EXPECT_FALSE(gcm.start(nullptr, 0));
}

TEST(gt_aaes, gcm_finish_before_start_rejected) {
    uint_8 key[16]{}, iv[12]{};
    aes_gcm<128> gcm(key, iv);
    uint_8 tag[16];
    EXPECT_FALSE(gcm.finish(tag));
}

// The hardware path's answer to "does this CPU run it": aes_base keeps its own probe private
static bool pclmulqdq_available() {
#if defined(_MSC_VER)
    int info[4]{0};
    __cpuid(info, 1);
    return (info[2] & (1 << 1)) != 0;
#else
    return __builtin_cpu_supports("pclmul") != 0;
#endif
}

static inline uint_32 xorshift(uint_32& _state) {
    _state ^= _state << 13;
    _state ^= _state >> 17;
    _state ^= _state << 5;
    return _state;
}

namespace {
    // The hardware entries are private and ghash() is protected, so this shim is how a test reaches
    // the same dispatcher the modes call -- on a PCLMULQDQ CPU that is the hardware path
    struct ghash_dispatch : alx::aes_base {
        using aes_base::ghash;
    };
}

TEST(gt_aaes, ghash_hard_matches_soft) {
#if !TRY_AES_HARD
    GTEST_SKIP() << "TRY_AES_HARD is off: the hardware entries are empty bodies";
#else
    if (!pclmulqdq_available()) GTEST_SKIP() << "PCLMULQDQ is not available";

    const char* subkeys[] = {"00000000000000000000000000000000",
                             "66e94bd4ef8a2c3b884cfa59ca342b2e",
                             "ffffffffffffffffffffffffffffffff"};
    uint_32 state = 0X2468ACE0U;

    for (const char* subkey : subkeys) {
        uint_8 h[16];
        from_hex(subkey, h);

        for (int round = 0; round < 512; ++round) {
            uint_8 s_soft[16], s_hard[16], d[16];
            for (int i = 0; i < 16; ++i) {
                s_soft[i] = uint_8(xorshift(state));
                d[i] = uint_8(xorshift(state));
            }
            memcpy(s_hard, s_soft, 16);

            aes_base::ghash_soft(s_soft, h, d);
            ghash_dispatch::ghash(s_hard, h, d);
            ASSERT_EQ(memcmp(s_soft, s_hard, 16), 0) << "subkey " << subkey << ", round " << round;
        }

        // every single-bit block against a zero state, then every single-bit state against a zero
        // block: bit order is the one thing the two implementations can disagree on without ever
        // failing on a busy input
        for (int bit = 0; bit < 128; ++bit) {
            uint_8 s_soft[16]{}, s_hard[16], d[16]{};
            d[bit >> 3] = uint_8(0X80U >> (bit & 7));
            memcpy(s_hard, s_soft, 16);
            aes_base::ghash_soft(s_soft, h, d);
            ghash_dispatch::ghash(s_hard, h, d);
            ASSERT_EQ(memcmp(s_soft, s_hard, 16), 0) << "subkey " << subkey << ", block bit " << bit;
        }

        for (int bit = 0; bit < 128; ++bit) {
            uint_8 s_soft[16]{}, s_hard[16], d[16]{};
            s_soft[bit >> 3] = uint_8(0X80U >> (bit & 7));
            memcpy(s_hard, s_soft, 16);
            aes_base::ghash_soft(s_soft, h, d);
            ghash_dispatch::ghash(s_hard, h, d);
            ASSERT_EQ(memcmp(s_soft, s_hard, 16), 0) << "subkey " << subkey << ", state bit " << bit;
        }
    }
#endif
}
