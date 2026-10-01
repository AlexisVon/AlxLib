/*****************************************************************/ /**
 * \file   gt_acompress.cpp
 * \brief  Unit tests for lz4 / gzip codecs
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "acompress.h"
#include <gtest/gtest.h>
#include <string>

using namespace alx;

TEST(gt_acompress, gzip_roundtrip) {
    const std::string src(4096, 'x');
    bytes in(src.data(), src.size());
    bytes packed = compress::encoder_gzip::sexec(in);
    ASSERT_FALSE(packed.empty());
    EXPECT_EQ(compress::decoder_gzip::sexec(packed), in);
}

TEST(gt_acompress, gzip_rejects_negative_size) {
    uint_8 gz[23] = {0x1f, 0x8b, 0x08};
    EXPECT_TRUE(compress::decoder_gzip::sexec(gz, -1).empty());
}

TEST(gt_acompress, negative_size_rejected_before_any_allocation) {
    const std::string src_text(4096, 'x');
    bytes src(src_text.data(), src_text.size());
    bytes packed = compress::encoder_gzip::sexec(src);
    ASSERT_FALSE(packed.empty());

    bytes out;
    EXPECT_TRUE(compress::encoder_gzip::sexec(src.data(), -1).empty());
    EXPECT_FALSE(compress::encoder_gzip::sexec(src.data(), -1, out));
    compress::encoder_gzip enc;
    EXPECT_TRUE(enc.exec(src.data(), -1).empty());

    EXPECT_TRUE(compress::decoder_gzip::sexec(src.data(), -1).empty());
    EXPECT_FALSE(compress::decoder_gzip::sexec(src.data(), -1, out));
    compress::decoder_gzip dec;
    EXPECT_TRUE(dec.exec(src.data(), -1).empty());

    EXPECT_TRUE(compress::decoder_lz4::sexec(packed.data(), -1, (int_32) src.size()).empty());
    compress::decoder_lz4 lz4_dec;
    EXPECT_TRUE(lz4_dec.exec(packed.data(), -1, (int_32) src.size()).empty());

    EXPECT_TRUE(compress::encoder_lz4::sexec(src.data(), -1).empty());
    EXPECT_FALSE(compress::encoder_lz4::sexec(src.data(), -1, out));
    compress::encoder_lz4 lz4_enc;
    EXPECT_TRUE(lz4_enc.exec(src.data(), -1).empty());
}

TEST(gt_acompress, gzip_oversized_estimate_dos_not_int_overflow) {

    uint_8 gz[23] = {0x1f, 0x8b, 0x08};
    EXPECT_TRUE(compress::decoder_gzip::sexec(gz, 0x20000000).empty());
}

namespace {

    bytes lcg_blob() {
        std::string blob;
        uint_32 x = 0x12345678U;
        for (int i = 0; i < 16384; i++) {
            x = x * 1664525U + 1013904223U;
            blob.push_back((char) (x >> 24));
        }
        return bytes(blob.data(), blob.size());
    }

    bytes zstd_header_claiming(uint_64 _content_size) {
        uint_8 head[13] = {0x28, 0xB5, 0x2F, 0xFD, 0xE0};
        for (int i = 0; i < 8; i++) head[5 + i] = (uint_8) (_content_size >> (8 * i));
        return bytes(head, sizeof(head));
    }
}

TEST(gt_acompress, zstd_roundtrip) {
    const std::string src(4096, 'x');
    bytes in(src.data(), src.size());
    bytes packed = compress::encoder_zstd::sexec(in);
    ASSERT_FALSE(packed.empty());
    EXPECT_EQ(compress::decoder_zstd::sexec(packed), in);
    EXPECT_LT(packed.size(), in.size());
}

TEST(gt_acompress, zstd_levels_roundtrip) {
    const std::string src(4096, 'x');
    bytes in(src.data(), src.size());
    for (uint_16 level : {0, 1, 3, 22, 65535}) {
        bytes packed = compress::encoder_zstd::sexec(in, level);
        ASSERT_FALSE(packed.empty()) << "level " << level;
        EXPECT_EQ(compress::decoder_zstd::sexec(packed), in) << "level " << level;
    }
}

TEST(gt_acompress, zstd_stream_is_one_open_frame) {
    const std::string text(8192, 'y');
    bytes in(text.data(), text.size());
    const bytes half1(in.data(), in.size() / 2);
    const bytes half2(in.data() + in.size() / 2, in.size() - in.size() / 2);

    compress::encoder_zstd enc;
    bytes c1 = enc.exec(half1);
    ASSERT_FALSE(c1.empty());

    compress::decoder_zstd dec;
    EXPECT_EQ(dec.exec(c1), half1);
    EXPECT_TRUE(compress::decoder_zstd::sexec(c1).empty());

    bytes c2 = enc.exec(half2);
    ASSERT_FALSE(c2.empty());
    EXPECT_EQ(dec.exec(c2), half2);

    bytes cat = c1;
    cat.append(c2.data(), c2.size());
    compress::decoder_zstd dec2;
    EXPECT_EQ(dec2.exec(cat), in);
    EXPECT_TRUE(compress::decoder_zstd::sexec(cat).empty());
}

TEST(gt_acompress, zstd_clear_cuts_dictionary) {
    const bytes payload = lcg_blob();

    compress::encoder_zstd enc;
    bytes first = enc.exec(payload);
    ASSERT_FALSE(first.empty());

    bytes second = enc.exec(payload);
    ASSERT_FALSE(second.empty());
    EXPECT_LT(second.size(), first.size() / 4);

    enc.clear();
    bytes after_clear = enc.exec(payload);
    ASSERT_FALSE(after_clear.empty());
    EXPECT_GT(after_clear.size(), first.size() / 2);

    compress::decoder_zstd fresh;
    EXPECT_EQ(fresh.exec(after_clear), payload);
}

TEST(gt_acompress, zstd_rejects_bad_input) {
    const std::string src_text(512, 'q');
    bytes src(src_text.data(), src_text.size());
    bytes packed = compress::encoder_zstd::sexec(src);
    ASSERT_FALSE(packed.empty());

    EXPECT_TRUE(compress::decoder_zstd::sexec(nullptr, 0).empty());
    EXPECT_TRUE(compress::decoder_zstd::sexec(packed.data(), -1).empty());
    EXPECT_TRUE(compress::decoder_zstd::sexec(packed.left_view(packed.size() - 4)).empty());

    bytes head_broken = packed;
    head_broken.data()[4] ^= 0xFF;
    EXPECT_TRUE(compress::decoder_zstd::sexec(head_broken).empty());

    bytes body_broken = packed;
    body_broken.data()[body_broken.size() / 2] ^= 0xFF;
    const bytes out = compress::decoder_zstd::sexec(body_broken);
    EXPECT_TRUE(out.empty() || out != src);
}

TEST(gt_acompress, zstd_forged_content_size_does_not_reserve) {

    const bytes modest = zstd_header_claiming(64ULL * 1024 * 1024);
    bytes out;
    EXPECT_FALSE(compress::decoder_zstd::sexec(modest.data(), (int_32) modest.size(), out));
    EXPECT_EQ(out.size(), 64ULL * 1024 * 1024);

    const bytes bomb = zstd_header_claiming(2ULL * 1024 * 1024 * 1024);
    bytes refused;
    EXPECT_FALSE(compress::decoder_zstd::sexec(bomb.data(), (int_32) bomb.size(), refused));
    EXPECT_TRUE(refused.empty());
    EXPECT_TRUE(compress::decoder_zstd::sexec(bomb).empty());
}

TEST(gt_acompress, gzip_claimed_length_must_not_wrap) {

    const std::string src_text(4096, 'x');
    bytes src(src_text.data(), src_text.size());
    bytes gz = compress::encoder_gzip::sexec(src);
    ASSERT_FALSE(gz.empty());

}

TEST(gt_acompress, zstd_frame_without_content_size_is_readable) {

    const uint_8 frame[10] = {0x28, 0xB5, 0x2F, 0xFD, 0x00, 0x00, 0x09, 0x00, 0x00, 0x61};
    const bytes one = compress::decoder_zstd::sexec(frame, (int_32) sizeof(frame));
    ASSERT_FALSE(one.null());
    EXPECT_EQ(one, bytes("a", 1));
}

TEST(gt_acompress, zstd_exec_stops_at_the_first_frame) {

    const std::string src_text(4096, 'y');
    bytes src(src_text.data(), src_text.size());
    bytes one = compress::encoder_zstd::sexec(src);
    ASSERT_FALSE(one.empty());

    bytes two = one;
    two.append(one.data(), one.size());
    compress::decoder_zstd dec;
    EXPECT_EQ(dec.exec(two), src);
}
