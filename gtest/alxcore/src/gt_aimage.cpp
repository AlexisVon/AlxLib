/*****************************************************************/ /**
 * \file   gt_aimage.cpp
 * \brief  Unit tests for image: pixel access, BMP round-trip
 *
 * \author alexis
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "aimage.h"
#include <cstring>
#include <gtest/gtest.h>

using namespace alx;

TEST(gt_aimage, create_image_32bit) {
    auto* img = image_base::create_image(64, 32, 32);
    ASSERT_NE(img, nullptr);
    EXPECT_EQ(img->width(), 64);
    EXPECT_EQ(img->height(), 32);
    EXPECT_EQ(img->depth(), 32);
    delete img;
}

TEST(gt_aimage, create_image_24bit) {
    auto* img = image_base::create_image(16, 16, 24);
    ASSERT_NE(img, nullptr);
    EXPECT_EQ(img->width(), 16);
    EXPECT_EQ(img->height(), 16);
    delete img;
}

TEST(gt_aimage, create_image_8bit) {
    auto* img = image_base::create_image(8, 8, 8);
    ASSERT_NE(img, nullptr);
    EXPECT_EQ(img->depth(), 8);
    delete img;
}

TEST(gt_aimage, to_bmp_roundtrip) {
    auto* img = image_base::create_image(10, 10, 24);
    ASSERT_NE(img, nullptr);

    bytes bmp_data = img->to_bmp();
    EXPECT_GT(bmp_data.size(), 0);

    auto* loaded = image_base::load_image_bmp(bmp_data);
    ASSERT_NE(loaded, nullptr);
    EXPECT_EQ(loaded->width(), 10);
    EXPECT_EQ(loaded->height(), 10);

    delete loaded;
    delete img;
}

TEST(gt_aimage, create_image_1bit) {
    auto* img = image_base::create_image(16, 16, 1);
    ASSERT_NE(img, nullptr);
    EXPECT_EQ(img->depth(), 1);
    delete img;
}

TEST(gt_aimage, create_image_invalid_depth) {

    auto* img = image_base::create_image(8, 8, 3);
    EXPECT_EQ(img, nullptr);
}

TEST(gt_aimage, to_bmp_has_valid_header) {
    auto* img = image_base::create_image(4, 4, 24);
    ASSERT_NE(img, nullptr);

    bytes bmp = img->to_bmp();
    ASSERT_GE(bmp.size(), 14);
    EXPECT_EQ(bmp.data()[0], 'B');
    EXPECT_EQ(bmp.data()[1], 'M');

    delete img;
}

TEST(gt_aimage, load_image_bmp_invalid) {

    bytes junk(100, '\0');
    auto* img = image_base::load_image_bmp(junk);
    EXPECT_EQ(img, nullptr);
    delete img;
}

TEST(gt_aimage, load_image_bmp_rejects_out_of_range_offset) {

    auto bmp = bytes(70, 0);
    bmp.data()[0] = 'B';
    bmp.data()[1] = 'M';
    uint_32 useclr = 1000, ofst = 54 + useclr * 4;
    memcpy(bmp.data() + 10, &ofst, 4);
    uint_32 mesg_size = 40, w = 1, h = 1;
    memcpy(bmp.data() + 14, &mesg_size, 4);
    memcpy(bmp.data() + 18, &w, 4);
    memcpy(bmp.data() + 22, &h, 4);
    uint_16 depth = 8;
    memcpy(bmp.data() + 28, &depth, 2);
    memcpy(bmp.data() + 46, &useclr, 4);

    EXPECT_EQ(image_base::load_image_bmp(bmp), nullptr) << "palette read must stay inside the buffer";
}

TEST(gt_aimage, load_image_bmp_rejects_truncated_pixels) {

    auto bmp = bytes(70, 0);
    bmp.data()[0] = 'B';
    bmp.data()[1] = 'M';
    uint_32 ofst = 54, dsize = 1000, mesg_size = 40, w = 1, h = 1;
    memcpy(bmp.data() + 10, &ofst, 4);
    memcpy(bmp.data() + 14, &mesg_size, 4);
    memcpy(bmp.data() + 18, &w, 4);
    memcpy(bmp.data() + 22, &h, 4);
    uint_16 depth = 8;
    memcpy(bmp.data() + 28, &depth, 2);
    memcpy(bmp.data() + 34, &dsize, 4);

    EXPECT_EQ(image_base::load_image_bmp(bmp), nullptr) << "row copy must stay inside the buffer";
}
