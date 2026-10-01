/*****************************************************************/ /**
 * \file   gt_abytes.cpp
 * \brief  Unit tests for bytes / bytes_view
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "abase.h"
#include "abytes.h"
#include <climits>
#include <gtest/gtest.h>
#include <stdexcept>

TEST(gt_abytes, constructor_default) {
    alx::bytes b;
    ASSERT_TRUE(b.null());
    ASSERT_TRUE(b.empty());
    ASSERT_EQ(b.size(), 0);
    ASSERT_EQ(b.capacity(), 0);
    ASSERT_EQ(b.data(), nullptr);
}

TEST(gt_abytes, construct_by_memory) {
    const int ival = 0x12345678;
    const void* src = &ival;
    alx::bytes b(src, sizeof(ival));
    ASSERT_FALSE(b.null());
    ASSERT_FALSE(b.empty());
    ASSERT_EQ(b.size(), sizeof(ival));
    ASSERT_GE(b.capacity(), sizeof(ival));
    ASSERT_EQ(b.to<int>(), ival);
}

TEST(gt_abytes, constructor_by_size) {
    alx::bytes b(10);
    ASSERT_FALSE(b.null());
    ASSERT_FALSE(b.empty());
    ASSERT_EQ(b.size(), 10);
    ASSERT_GE(b.capacity(), 10);
}

TEST(gt_abytes, constructor_by_size_value) {
    alx::bytes b(10, 'a');
    ASSERT_FALSE(b.null());
    ASSERT_FALSE(b.empty());
    ASSERT_EQ(b.size(), 10);
    ASSERT_GE(b.capacity(), 10);
    ASSERT_EQ(b.to_string(), std::string(10, 'a'));
}

TEST(gt_abytes, construct_by_cstr) {
    const char src[] = "abcdefghijklmnopqrstuvwxyz";
    alx::bytes b(src);
    ASSERT_FALSE(b.null());
    ASSERT_FALSE(b.empty());
    ASSERT_EQ(b.size(), sizeof(src) - 1);
    ASSERT_GE(b.capacity(), sizeof(src) - 1);
    ASSERT_EQ(b.to_string(), src);
}

TEST(gt_abytes, construct_by_string) {
    const std::string src = "abcdefghijklmnopqrstuvwxyz";
    alx::bytes b(src);
    ASSERT_FALSE(b.null());
    ASSERT_FALSE(b.empty());
    ASSERT_EQ(b.size(), src.size());
    ASSERT_GE(b.capacity(), src.size());
    ASSERT_EQ(b.to_string(), src);
}

TEST(gt_abytes, construct_by_copy) {
    const char src[] = "abcdefghijklmnopqrstuvwxyz";
    alx::bytes b(src);
    ASSERT_EQ(b.to_string(), src);
    alx::bytes cb(b);
    ASSERT_FALSE(cb.null());
    ASSERT_FALSE(cb.empty());
    ASSERT_EQ(cb.size(), sizeof(src) - 1);
    ASSERT_GE(cb.capacity(), sizeof(src) - 1);
    ASSERT_EQ(cb.to_string(), src);
}

TEST(gt_abytes, construct_by_move) {
    const char src[] = "abcdefghijklmnopqrstuvwxyz";
    alx::bytes b(src);
    ASSERT_EQ(b.to_string(), src);
    alx::bytes mb(std::move(b));
    EXPECT_TRUE(b.null());
    ASSERT_FALSE(mb.null());
    ASSERT_FALSE(mb.empty());
    ASSERT_EQ(mb.size(), sizeof(src) - 1);
    ASSERT_GE(mb.capacity(), sizeof(src) - 1);
    ASSERT_EQ(mb.to_string(), src);
}

TEST(gt_abytes, assign_by_copy) {
    const char src[] = "abcdefghijklmnopqrstuvwxyz";
    alx::bytes b(src);
    ASSERT_EQ(b.to_string(), src);
    alx::bytes cb;
    ASSERT_TRUE(cb.null());
    cb = b;
    ASSERT_FALSE(cb.null());
    ASSERT_FALSE(cb.empty());
    ASSERT_EQ(cb.size(), sizeof(src) - 1);
    ASSERT_GE(cb.capacity(), sizeof(src) - 1);
    ASSERT_EQ(cb.to_string(), src);
}

TEST(gt_abytes, assign_by_move) {
    const char src[] = "abcdefghijklmnopqrstuvwxyz";
    alx::bytes b(src);
    ASSERT_EQ(b.to_string(), src);
    alx::bytes mb;
    ASSERT_TRUE(mb.null());
    mb = std::move(b);
    EXPECT_TRUE(b.null());
    ASSERT_FALSE(mb.null());
    ASSERT_FALSE(mb.empty());
    ASSERT_EQ(mb.size(), sizeof(src) - 1);
    ASSERT_GE(mb.capacity(), sizeof(src) - 1);
    ASSERT_EQ(mb.to_string(), src);
}

TEST(gt_abytes, base_functions) {
    const char src[] = "abcdefghijklmnopqrstuvwxyz";
    alx::bytes b(src, sizeof(src));
    ASSERT_FALSE(b.empty());
    ASSERT_FALSE(b.null());
    ASSERT_GE(b.capacity(), sizeof(src));
    ASSERT_STREQ((const char*) b.cdata(), src);
    ASSERT_EQ(b.size(), sizeof(src));
    b.resize(10);
    ASSERT_EQ(b.size(), 10);
    b.reserve(256);
    ASSERT_GE(b.capacity(), 256);
    b.fitsize();
    EXPECT_LT(b.capacity(), 256);
    b.clear();
    ASSERT_TRUE(b.empty());
}

TEST(gt_abytes, base_operators) {
    const char src[] = "abcdefghijklmnopqrstuvwxyz";
    alx::bytes b(src, sizeof(src));
    ASSERT_EQ(b[0], 'a');
    ASSERT_EQ(b[sizeof(src) - 1], '\0');
    alx::bytes b2(src, sizeof(src));
    ASSERT_EQ(b, b2);
    b2[0] = 'A';
    ASSERT_NE(b, b2);
}

TEST(gt_abytes, iterator) {
    const char src[] = "abcdefghijklmnopqrstuvwxyz";
    alx::bytes b(src);
    const alx::bytes& cb = b;
    const char* ptr = src;
    for (auto it = b.begin(); it != b.end(); ++it) {
        ASSERT_EQ(*it, *ptr);
        ++ptr;
    }
    ptr = src;
    for (auto it = b.cbegin(); it != b.cend(); ++it) {
        ASSERT_EQ(*it, *ptr);
        ++ptr;
    }
    ptr = src;
    for (auto it = cb.begin(); it != cb.end(); ++it) {
        ASSERT_EQ(*it, *ptr);
        ++ptr;
    }
    ptr = src;
    for (auto it : cb) {
        ASSERT_EQ(it, *ptr);
        ++ptr;
    }
}

TEST(gt_abytes, detach_func) {
    alx::bytes b("1234567890");
    alx::bytes cb(b);
    ASSERT_EQ(cb.cdata(), b.cdata());
    cb.detach();
    ASSERT_NE(cb.cdata(), b.cdata());
}

TEST(gt_abytes, detach_by_data) {
    alx::bytes b("1234567890");
    alx::bytes cb(b);
    ASSERT_EQ(cb.cdata(), b.cdata());
    ASSERT_NE(cb.data(), b.data());
}

TEST(gt_abytes, detach_by_resize) {
    alx::bytes b("1234567890");
    alx::bytes cb(b);
    ASSERT_EQ(cb.cdata(), b.cdata());
    cb.resize(cb.capacity());
    EXPECT_NE(cb.cdata(), b.cdata());
    EXPECT_EQ(b.size(), 10);
    cb.resize(cb.capacity() + 1);
    ASSERT_NE(cb.cdata(), b.cdata());
}

TEST(gt_abytes, resize_keeps_aliases) {
    alx::bytes a("1234567890");
    alx::bytes b(a);
    b.resize(4);
    EXPECT_EQ(a.size(), 10);
    EXPECT_EQ(a.to_string(), "1234567890");
    EXPECT_EQ(b.size(), 4);
    b.resize(0);
    a.append("XY");
    EXPECT_EQ(a.size(), 12);
    EXPECT_EQ(a.to_string(), "1234567890XY");
}

TEST(gt_abytes, detach_by_reserve) {
    alx::bytes b("1234567890");
    alx::bytes cb(b);
    ASSERT_EQ(cb.cdata(), b.cdata());
    cb.reserve(cb.capacity());
    EXPECT_EQ(cb.cdata(), b.cdata());
    cb.reserve(cb.capacity() + 1);
    ASSERT_NE(cb.cdata(), b.cdata());
}

TEST(gt_abytes, detach_by_raccess) {
    alx::bytes b("1234567890");
    alx::bytes cb(b);
    ASSERT_EQ(cb.cdata(), b.cdata());
    cb[cb.size() - 1] = 0X7FU;
    ASSERT_EQ(cb[cb.size() - 1], 0X7FU);
    EXPECT_NE(cb.cdata(), b.cdata());
}

TEST(gt_abytes, about_hex) {
    alx::bytes b = alx::bytes::from_hex("123456789ABCDEF0");
    ASSERT_EQ(b.size(), 8);
    ASSERT_EQ(b[0], 0X12);
    ASSERT_EQ(b[1], 0X34);
    ASSERT_EQ(b[2], 0X56);
    ASSERT_EQ(b[3], 0X78);
    ASSERT_EQ(b[4], 0X9A);
    ASSERT_EQ(b[5], 0XBC);
    ASSERT_EQ(b[6], 0XDE);
    ASSERT_EQ(b[7], 0XF0);
    ASSERT_EQ(b.to_hex(), "123456789abcdef0");
    alx::bytes c = alx::bytes::from_hex("123456789ABCDEF");
    ASSERT_TRUE(c.null());
    alx::bytes d = alx::bytes::from_hex("123456789ABCDEFG");
    ASSERT_TRUE(d.null());
}

TEST(gt_abytes, about_base64) {
    alx::bytes b = alx::bytes::from_base64("MTIzNDU2Nzg5");
    ASSERT_EQ(b.size(), 9);
    ASSERT_EQ(b[0], 0X31);
    ASSERT_EQ(b[1], 0X32);
    ASSERT_EQ(b[2], 0X33);
    ASSERT_EQ(b[3], 0X34);
    ASSERT_EQ(b[4], 0X35);
    ASSERT_EQ(b[5], 0X36);
    ASSERT_EQ(b[6], 0X37);
    ASSERT_EQ(b[7], 0X38);
    ASSERT_EQ(b[8], 0X39);
    ASSERT_EQ(b.to_base64(), "MTIzNDU2Nzg5");
    alx::bytes c = alx::bytes::from_base64("MTIzNDU2Nzg");
    ASSERT_TRUE(c.null());
    alx::bytes d = alx::bytes::from_base64("MTIzNDU2Nz");
    ASSERT_TRUE(d.null());
}

TEST(gt_abytes, about_ordinary) {
    struct T {
        alx::uint_64 a{alx::max_uint_64};
        alx::uint_32 b{alx::min_uint_32};
        alx::uint_16 c{alx::max_uint_16};
        alx::uint_8 d{alx::min_uint_8_};
        alx::uint_8 e{alx::max_uint_8_};
    } t;
    alx::bytes b = alx::bytes::from_ordinary(t);
    ASSERT_EQ(b.size(), sizeof(T));
    T t2 = b.to<T>();
    ASSERT_EQ(t.a, t2.a);
    ASSERT_EQ(t.b, t2.b);
    ASSERT_EQ(t.c, t2.c);
    ASSERT_EQ(t.d, t2.d);
    ASSERT_EQ(t.e, t2.e);
}

TEST(gt_abytes, append) {
    alx::bytes b;
    b.append("hello").append(" world").append(alx::bytes::from_hex("3132")).append_ordinary(alx::uint_64(0X3334353637383930));
    ASSERT_EQ(b.to_string(), "hello world1209876543");
}

TEST(gt_abytes, operator_lshift) {
    alx::bytes b;
    b << "hello" << " world" << alx::bytes::from_hex("3132") << alx::uint_64(0X3334353637383930);
    ASSERT_EQ(b.to_string(), "hello world1209876543");
}

TEST(gt_abytes, mid) {
    alx::bytes b("0123456789");
    ASSERT_EQ(b.mid(0).to_string(), "0123456789");
    ASSERT_EQ(b.mid(2, 4).to_string(), "2345");
    ASSERT_EQ(b.mid(2).to_string(), "23456789");
    ASSERT_EQ(b.mid(2, 9).to_string(), "23456789");
    ASSERT_EQ(b.mid(2, -1).to_string(), "23456789");
    ASSERT_EQ(b.mid(-1).to_string(), "");
    alx::bytes eb;
    ASSERT_EQ(eb.mid(0), eb);
    ASSERT_EQ(eb.mid(2, 4), eb);
    ASSERT_EQ(eb.mid(2), eb);
    ASSERT_EQ(eb.mid(2, 9), eb);
    ASSERT_EQ(eb.mid(2, -1), eb);
    ASSERT_EQ(eb.mid(-1), eb);
}

TEST(gt_abytes, right) {
    alx::bytes b("0123456789");
    ASSERT_EQ(b.right(0).to_string(), "");
    ASSERT_EQ(b.right(2).to_string(), "89");
    ASSERT_EQ(b.right(-1).to_string(), "0123456789");
    alx::bytes eb;
    ASSERT_EQ(eb.right(0), eb);
    ASSERT_EQ(eb.right(2), eb);
    ASSERT_EQ(eb.right(-1), eb);
}

TEST(gt_abytes, left) {
    alx::bytes b("0123456789");
    ASSERT_EQ(b.left(0).to_string(), "");
    ASSERT_EQ(b.left(2).to_string(), "01");
    ASSERT_EQ(b.left(-1).to_string(), "0123456789");
    alx::bytes eb;
    ASSERT_EQ(eb.left(0), eb);
    ASSERT_EQ(eb.left(2), eb);
    ASSERT_EQ(eb.left(-1), eb);
}

TEST(gt_abytes, find) {
    alx::bytes b("01234567898765432100123456789876543210");
    EXPECT_EQ(b.find("0", 1), 0);
    EXPECT_EQ(b.find("0", 1, 0), 0);
    EXPECT_EQ(b.find("0", 1, 1), 18);
    EXPECT_EQ(b.find("0", 1, 1, 18), -1);
    EXPECT_EQ(b.find("0", 1, 19), 19);
    EXPECT_EQ(b.find("0", 1, 20), 37);
    EXPECT_EQ(b.find("0", 1, 20, 37), -1);
    EXPECT_EQ(b.find("0", 1, 38), -1);
    EXPECT_EQ(b.find_ordinary(alx::uint_16(0X3736U)), 6);
    EXPECT_EQ(b.find("", 0), 0);
    EXPECT_EQ(b.find("", 0, 1), 1);
}

TEST(gt_abytes, rfind) {
    alx::bytes b("01234567898765432100123456789876543210");
    EXPECT_EQ(b.rfind("0", 1), 37);
    EXPECT_EQ(b.rfind("0", 1, -1), 37);
    EXPECT_EQ(b.rfind("0", 1, 36), 19);
    EXPECT_EQ(b.rfind("0", 1, 36, 20), -1);
    EXPECT_EQ(b.rfind("0", 1, 18), 18);
    EXPECT_EQ(b.rfind("0", 1, 17), 0);
    EXPECT_EQ(b.rfind("0", 1, 17, 1), -1);
    EXPECT_EQ(b.rfind("0", 1, 0), 0);
    EXPECT_EQ(b.rfind_ordinary(alx::uint_16(0X3736U)), 25);
    EXPECT_EQ(b.rfind("", 0), 37);
    EXPECT_EQ(b.rfind("", 0, 1), 1);
}

TEST(gt_abytes, equa) {
    alx::bytes b("01234567898765432100123456789876543210");
    EXPECT_TRUE(b.equa("01234567898765432100123456789876543210", 38, 0));
    EXPECT_TRUE(b.equa("", 0, 0));
    EXPECT_TRUE(b.equa("", 0, -1));
    EXPECT_TRUE(b.equa("1234", 4, 1));
    EXPECT_TRUE(b.equa("1234", 4, 20));
}

TEST(gt_abytes_view, constructor) {
    alx::bytes b("01234567898765432100123456789876543210");
    alx::bytes_view v(b), v2(b, 10, 10);
    ASSERT_EQ(v, b);
    ASSERT_EQ(v2, b.mid(10, 10));
    alx::bytes_view v3;
    ASSERT_TRUE(v3.null());
    ASSERT_TRUE(v3.empty());
    ASSERT_EQ(v3.size(), 0);
    ASSERT_EQ(v3.data(), nullptr);
}

TEST(gt_abytes_view, assign) {
    alx::bytes b("01234567898765432100123456789876543210");
    alx::bytes_view v(b), v2(b, 10, 10), v3(b, 12, 8);
    ASSERT_EQ(v, b);
    ASSERT_EQ(v2, b.mid(10, 10));
    ASSERT_EQ(v3, b.mid(12, 8));
    v = v2;
    ASSERT_EQ(v, b.mid(10, 10));
    v = b;
    ASSERT_EQ(v, b);
    v = std::move(v3);
    EXPECT_TRUE(v3.empty());
    ASSERT_EQ(v, b.mid(12, 8));
}

TEST(gt_abytes_view, from_sub_bytes) {
    alx::bytes b("01234567898765432100123456789876543210");
    ASSERT_EQ(b.mid(0), b.mid_view(0));
    ASSERT_EQ(b.mid(0, 10), b.mid_view(0, 10));
    ASSERT_EQ(b.mid(10), b.mid_view(10));
    ASSERT_EQ(b.mid(10, 10), b.mid_view(10, 10));
    ASSERT_EQ(b.mid(20), b.mid_view(20));
    ASSERT_EQ(b.mid(20, 10), b.mid_view(20, 10));
    ASSERT_EQ(b.left(0), b.left_view(0));
    ASSERT_EQ(b.left(10), b.left_view(10));
    ASSERT_EQ(b.right(0), b.right_view(0));
    ASSERT_EQ(b.right(10), b.right_view(10));
}

TEST(gt_abytes, revers_empty) {
    alx::bytes b;
    b.revers();
    EXPECT_TRUE(b.empty());
}

TEST(gt_abytes, revers_single_byte) {
    alx::bytes b(1, 'A');
    b.revers();
    EXPECT_EQ(b[0], 'A');
}

TEST(gt_abytes, revers_small) {

    alx::bytes b("0123456789");
    alx::bytes orig = b;
    b.revers();
    for (size_t i = 0; i < b.size(); i++)
        EXPECT_EQ(b[i], orig[orig.size() - 1 - i]);
}

TEST(gt_abytes, revers_large) {

    alx::bytes b(100, 0);
    for (size_t i = 0; i < 100; i++) b.data()[i] = (alx::uint_8) i;
    alx::bytes orig = b;
    b.revers();
    for (size_t i = 0; i < 100; i++)
        EXPECT_EQ(b[i], orig[99 - i]);
}

TEST(gt_abytes, revers_large_non_multiple_of_8) {
    alx::bytes b(31, 0);
    for (size_t i = 0; i < 31; i++) b.data()[i] = (alx::uint_8) (i + 1);
    alx::bytes orig = b;
    b.revers();
    for (size_t i = 0; i < 31; i++)
        EXPECT_EQ(b[i], orig[30 - i]);
}

TEST(gt_abytes, revers_static_ptr) {

    alx::uint_8 data[] = {0, 1, 2, 3, 4, 5, 6, 7};
    alx::bytes::revers(data, 8);
    EXPECT_EQ(data[0], 7);
    EXPECT_EQ(data[1], 6);
    EXPECT_EQ(data[7], 0);
}
TEST(gt_abytes, an_unrepresentable_size_throws) {

    alx::bytes kept(32, (alx::uint_8) 7);
    const char text[] = "abc";
    EXPECT_THROW(kept.append(text, alx::max_uint_64), std::length_error);
    EXPECT_EQ(kept.size(), 32u);
    EXPECT_EQ(kept.data()[0], 7);

    EXPECT_THROW(kept.resize(alx::max_uint_64 - 8), std::length_error);
    EXPECT_EQ(kept.size(), 32u);

    EXPECT_THROW(alx::bytes wrapped(alx::max_uint_64 - 24), std::length_error);
}
