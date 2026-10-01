/*****************************************************************/ /**
 * \file   gt_acsv.cpp
 * \brief  Unit tests for the CSV text matrix
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "acsv.h"
#include <cstring>
#include <gtest/gtest.h>
#include <string>

using namespace alx;

namespace {
    varvec table_of(std::initializer_list<std::initializer_list<const char*>> _rows) {
        varvec rows;
        for (const auto& src : _rows) {
            varvec row;
            for (const char* cell : src) row.push_back(std::string(cell));
            rows.push_back(row);
        }
        return rows;
    }

    std::string text_of(const bytes& _bytes) {
        return std::string((const char*) _bytes.data(), (size_t) _bytes.size());
    }

    std::string cell_at(const varvec& _rows, size_t _r, size_t _c) {
        return _rows[_r].to<varvec>()[_c].to<std::string>();
    }

    size_t row_size(const varvec& _rows, size_t _r) {
        return _rows[_r].to<varvec>().size();
    }
}

TEST(gt_acsv, roundtrip) {
    const varvec rows = table_of({{"name", "qty"}, {"apple", "3"}, {"pear", "12"}});

    const bytes text = csv::to_bytes(rows);
    EXPECT_EQ(text_of(text), "name,qty\napple,3\npear,12\n");

    bool ok = false;
    const varvec back = csv::from_bytes(text_of(text), ',', true, &ok);
    ASSERT_TRUE(ok);
    EXPECT_EQ(back, rows);
}

TEST(gt_acsv, quoting_and_line_breaks) {

    const varvec rows = table_of({{"k", "v"}, {"a,b", "say \"hi\"\nnow"}});

    const bytes text = csv::to_bytes(rows);
    EXPECT_EQ(text_of(text), "k,v\n\"a,b\",\"say \"\"hi\"\"\nnow\"\n");

    bool ok = false;
    const varvec back = csv::from_bytes(text_of(text), ',', true, &ok);
    ASSERT_TRUE(ok);
    EXPECT_EQ(back, rows);
}

TEST(gt_acsv, leading_blank_is_quoted) {

    const varvec rows = table_of({{"pad", "plain"}, {" x", "y "}});
    EXPECT_EQ(text_of(csv::to_bytes(rows)), "pad,plain\n\" x\",\"y \"\n");

    bool ok = false;
    EXPECT_EQ(csv::from_bytes(text_of(csv::to_bytes(rows)), ',', true, &ok), rows);
    EXPECT_TRUE(ok);
}

TEST(gt_acsv, line_endings_and_no_trailing_break) {
    const std::string cases[] = {"a,b\r\n1,2\r\n", "a,b\n1,2", "a,b\r1,2\r"};
    for (const std::string& text : cases) {
        bool ok = false;
        const varvec rows = csv::from_bytes(text, ',', true, &ok);
        ASSERT_TRUE(ok) << text;
        ASSERT_EQ(rows.size(), 2U) << text;
        EXPECT_EQ(cell_at(rows, 1, 0), "1") << text;
        EXPECT_EQ(cell_at(rows, 1, 1), "2") << text;
    }
}

TEST(gt_acsv, header_is_row_zero) {
    const varvec rows = csv::from_bytes(std::string("host,uptime\nweb1,99.9\n"));
    ASSERT_EQ(rows.size(), 2U);
    EXPECT_EQ(cell_at(rows, 0, 0), "host");
    EXPECT_EQ(cell_at(rows, 0, 1), "uptime");
}

TEST(gt_acsv, blank_lines_are_not_records) {
    bool ok = false;
    const varvec rows = csv::from_bytes(std::string("a,b\n\n1,2\n\n"), ',', true, &ok);
    ASSERT_TRUE(ok);
    EXPECT_EQ(rows.size(), 2U);
}

TEST(gt_acsv, strict_and_lenient) {
    const std::string ragged = "a,b\n1\n";
    bool ok = true;
    EXPECT_TRUE(csv::from_bytes(ragged, ',', true, &ok).empty());
    EXPECT_FALSE(ok);

    ok = false;
    const varvec loose = csv::from_bytes(ragged, ',', false, &ok);
    ASSERT_TRUE(ok);
    ASSERT_EQ(loose.size(), 2U);
    EXPECT_EQ(row_size(loose, 1), 1U);
}

TEST(gt_acsv, rejects_bad_input) {
    bool ok = true;
    EXPECT_TRUE(csv::from_bytes(std::string("\"unclosed"), ',', true, &ok).empty());
    EXPECT_FALSE(ok);

    ok = true;
    EXPECT_TRUE(csv::from_bytes(std::string("a\"b,c"), ',', true, &ok).empty());
    EXPECT_FALSE(ok);

    ok = true;
    EXPECT_TRUE(csv::from_bytes(std::string("\"a\"x,b"), ',', true, &ok).empty());
    EXPECT_FALSE(ok);

    ok = true;
    EXPECT_TRUE(csv::from_bytes(std::string("a,b"), '"', true, &ok).empty());
    EXPECT_FALSE(ok);

    ok = true;
    EXPECT_TRUE(csv::from_bytes(nullptr, 16, ',', true, &ok).empty());
    EXPECT_FALSE(ok);

    ok = false;
    EXPECT_TRUE(csv::from_bytes(std::string(""), ',', true, &ok).empty());
    EXPECT_TRUE(ok);
}

TEST(gt_acsv, gbk_and_bom) {

    const char* gbk = "a,b\n\xD6\xD0\xCE\xC4,1\n";
    bool ok = false;
    varvec rows = csv::from_bytes(gbk, (uint_64) std::strlen(gbk), ',', true, &ok);
    ASSERT_TRUE(ok);
    EXPECT_EQ(cell_at(rows, 1, 0), "\xE4\xB8\xAD\xE6\x96\x87");

    const char* bom = "\xEF\xBB\xBF"
                      "a,b\n1,2\n";
    ok = false;
    rows = csv::from_bytes(bom, (uint_64) std::strlen(bom), ',', true, &ok);
    ASSERT_TRUE(ok);
    EXPECT_EQ(cell_at(rows, 0, 0), "a");
}

TEST(gt_acsv, write_is_text_only) {
    varvec rows;
    varvec row;
    row.push_back(3);
    rows.push_back(row);
    EXPECT_TRUE(csv::to_bytes(rows).null());

    bytes sink;
    ostream_buff out(sink);
    EXPECT_FALSE(csv::to_bytes(rows, out));

    varvec text_rows = table_of({{"3"}});
    EXPECT_EQ(text_of(csv::to_bytes(text_rows)), "3\n");
}

TEST(gt_acsv, single_empty_cell_survives) {
    const varvec rows = table_of({{""}});
    const bytes text = csv::to_bytes(rows);
    EXPECT_EQ(text_of(text), "\"\"\n");

    bool ok = false;
    const varvec back = csv::from_bytes(text_of(text), ',', true, &ok);
    ASSERT_TRUE(ok);
    EXPECT_EQ(back, rows);
}

TEST(gt_acsv, stream_outlet_and_consuming_write) {
    varvec rows = table_of({{"h"}, {"v"}});
    bytes sink;
    ostream_buff out(sink);
    EXPECT_TRUE(csv::to_bytes(rows, out));
    EXPECT_EQ(text_of(sink), "h\nv\n");
    EXPECT_EQ(rows.size(), 2U);

    varvec moving = rows;
    const bytes text = csv::to_bytes(std::move(moving));
    EXPECT_EQ(text_of(text), "h\nv\n");
    EXPECT_TRUE(moving.empty());
}

TEST(gt_acsv, delimiter_must_be_ascii) {

    bool ok = true;
    EXPECT_TRUE(csv::from_bytes(std::string("a\x80"
                                            "b\n"),
                                (char) 0x80, false, &ok)
                    .empty());
    EXPECT_FALSE(ok);

    const varvec rows = table_of({{"a"}, {"b"}});
    EXPECT_TRUE(csv::to_bytes(rows, (char) 0x80).null());
    EXPECT_TRUE(csv::to_bytes(rows, '"').null());
}

TEST(gt_acsv, empty_table_is_not_the_failure_sentinel) {
    const bytes empty_table = csv::to_bytes(varvec{});
    EXPECT_FALSE(empty_table.null());
    EXPECT_TRUE(empty_table.empty());

    varvec bad;
    varvec row;
    row.push_back(3);
    bad.push_back(row);
    EXPECT_TRUE(csv::to_bytes(bad).null());
}

TEST(gt_acsv, bom_only_input_is_an_empty_table) {
    const char* bom = "\xEF\xBB\xBF";
    bool ok = false;
    const varvec rows = csv::from_bytes(bom, 3, ',', true, &ok);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(rows.empty());
}

TEST(gt_acsv, gbk_delimiter_is_not_swallowed) {

    bool ok = true;
    EXPECT_TRUE(csv::from_bytes(std::string("p\xB0|q|r\ns\xB0|t|u\n"), '|', true, &ok).empty());
    EXPECT_FALSE(ok);
}

TEST(gt_acsv, utf16_is_converted_whole) {

    const char u16[] = "\xFF\xFE"
                       "a\0,\0b\0\n\0"
                       "1\0,\0"
                       "2\0\n\0";
    bool ok = false;
    const varvec rows = csv::from_bytes(u16, (uint_64) sizeof(u16) - 1, ',', true, &ok);
    ASSERT_TRUE(ok);
    ASSERT_EQ(rows.size(), 2U);
    EXPECT_EQ(cell_at(rows, 1, 0), "1");
    EXPECT_EQ(cell_at(rows, 1, 1), "2");
}
