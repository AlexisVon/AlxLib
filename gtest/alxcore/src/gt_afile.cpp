/*****************************************************************/ /**
 * \file   gt_afile.cpp
 * \brief  Unit tests for file I/O and file-backed streams
 *
 * \author alexis
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "astream_ex.h"
#include <cstdio>
#include <gtest/gtest.h>

#ifndef _WIN32
#    include <chrono>
#    include <fcntl.h>
#    include <sys/stat.h>
#    include <thread>
#    include <unistd.h>
#endif

using namespace alx;

TEST(gt_afile, write_then_read_new_file) {
    const std::string path = "tmp-alxcore-gtest/alx_gt_afile_new.bin";
    std::remove(path.c_str());
    const std::string text = "hello alxlib";

    file wr;
    ASSERT_TRUE(wr.open(file_info(path), file::WRIT));
    ASSERT_TRUE(wr.write(text));
    wr.close();

    file rd;
    ASSERT_TRUE(rd.open(file_info(path), file::READ));
    EXPECT_EQ(rd.read().to_string(), text);

    std::remove(path.c_str());
}

TEST(gt_afile, append_is_visible_to_same_handle) {
    const std::string path = "tmp-alxcore-gtest/alx_gt_afile_append.bin";
    std::remove(path.c_str());
    ASSERT_TRUE(file::write_all(file_info(path), "abc", 3, false));

    file f;
    ASSERT_TRUE(f.open(file_info(path), file::APED | file::R__W));
    ASSERT_TRUE(f.write(std::string("def")));
    EXPECT_EQ(f.info().size(), 6u);
    EXPECT_EQ(f.read(3, -1).to_string(), "def");

    std::remove(path.c_str());
}

TEST(gt_afile, write_at_offset_keeps_info_size) {
    const std::string path = "tmp-alxcore-gtest/alx_gt_afile_offset.bin";
    std::remove(path.c_str());

    file f;
    ASSERT_TRUE(f.open(file_info(path), file::WRIT | file::R__W));
    ASSERT_TRUE(f.write(std::string("0123456789")));
    ASSERT_TRUE(f.write(std::string("XY"), 4));
    EXPECT_EQ(f.info().size(), 10u);
    EXPECT_EQ(f.read().to_string(), "0123XY6789");

    std::remove(path.c_str());
}

TEST(gt_afile, reopen_with_own_info) {
    const std::string path = "tmp-alxcore-gtest/alx_gt_afile_alias.bin";
    std::remove(path.c_str());
    ASSERT_TRUE(file::write_all(file_info(path), "payload", 7, false));

    file f;
    ASSERT_TRUE(f.open(file_info(path), file::READ));
    ASSERT_TRUE(f.open(f.info(), file::READ));
    EXPECT_EQ(f.read().to_string(), "payload");

    std::remove(path.c_str());
}

TEST(gt_afile, write_all_read_all_roundtrip) {
    const std::string path = "tmp-alxcore-gtest/alx_gt_afile_all.bin";
    std::remove(path.c_str());
    std::string big(256 * 1024 + 7, '\0');
    for (size_t i = 0; i < big.size(); i++) big[i] = (char) (i & 0X7FU);

    ASSERT_TRUE(file::write_all(file_info(path), big.data(), big.size(), false));
    EXPECT_EQ(file::read_all(file_info(path)).to_string(), big);
    EXPECT_EQ(file::read_all(file_info(path)).size(), big.size());

    std::remove(path.c_str());
}

TEST(gt_afile, read_all_missing_file) {
    EXPECT_TRUE(file::read_all(file_info("tmp-alxcore-gtest/alx_gt_afile_missing.bin")).empty());
}

TEST(gt_afile, read_all_empty_file_is_a_success) {
    const std::string path = "tmp-alxcore-gtest/alx_gt_afile_empty.bin";
    std::remove(path.c_str());
    ASSERT_TRUE(file::write_all(file_info(path), "", 0, false));

    bool ok = false;
    EXPECT_TRUE(file::read_all(file_info(path), &ok).empty());
    EXPECT_TRUE(ok);
    std::remove(path.c_str());
}

TEST(gt_afile, read_all_missing_file_reports_failure) {
    bool ok = true;
    EXPECT_TRUE(file::read_all(file_info("tmp-alxcore-gtest/alx_gt_afile_nope.bin"), &ok).empty());
    EXPECT_FALSE(ok);
}

TEST(gt_afile, read_past_eof_is_not_a_failure) {
    const std::string path = "tmp-alxcore-gtest/alx_gt_afile_eof.bin";
    ASSERT_TRUE(file::write_all(file_info(path), "abc", 3, false));

    file rd;
    ASSERT_TRUE(rd.open(file_info(path), file::READ));
    bool ok = false;
    EXPECT_TRUE(rd.read(4096, uint_64_npos, &ok).empty());
    EXPECT_TRUE(ok);
    rd.close();
    std::remove(path.c_str());
}

TEST(gt_afile, flush_reports_success) {
    const std::string path = "tmp-alxcore-gtest/alx_gt_afile_flush.bin";
    std::remove(path.c_str());

    file wr;
    ASSERT_TRUE(wr.open(file_info(path), file::WRIT | file::R__W));
    EXPECT_TRUE(wr.write("flushed", 7));
    EXPECT_TRUE(wr.flush());
    wr.close();
    std::remove(path.c_str());
}

#ifndef _WIN32
TEST(gt_afile, read_all_reads_a_fifo_to_eof) {
    const char* path = "tmp-alxcore-gtest/alx_gt_afile_fifo";
    std::remove(path);
    ASSERT_EQ(0, mkfifo(path, 0644));

    const std::string payload = "fifo-payload";
    std::thread writer([path, payload] {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        const int fd = ::open(path, O_WRONLY);
        if (fd >= 0) {
            (void) !::write(fd, payload.data(), payload.size());
            ::close(fd);
        }
    });

    bool ok = false;
    const bytes data = file::read_all(path, &ok);
    writer.join();
    EXPECT_TRUE(ok);
    EXPECT_EQ(data.to_string(), payload);

    std::remove(path);
}

TEST(gt_afile, positional_read_on_a_fifo_reports_failure) {
    const char* path = "tmp-alxcore-gtest/alx_gt_afile_fifo_seek";
    std::remove(path);
    ASSERT_EQ(0, mkfifo(path, 0644));

    std::thread holder([path] {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        const int fd = ::open(path, O_WRONLY);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        if (fd >= 0) ::close(fd);
    });

    file rd;
    ASSERT_TRUE(rd.open(file_info(path), file::READ));
    bool ok = true;
    EXPECT_TRUE(rd.read(0, uint_64_npos, &ok).empty());
    EXPECT_FALSE(ok);
    rd.close();
    holder.join();

    std::remove(path);
}
#endif

TEST(gt_astream_ex, ostream_file_reset_restarts_output) {
    const std::string path = "tmp-alxcore-gtest/alx_gt_ostream_reset.bin";
    std::remove(path.c_str());
    {
        ostream_file ostm(file_info(path), false);
        ASSERT_TRUE(ostm.append(std::string("first")));
        ostm.reset();
        EXPECT_EQ(ostm.total(), 0u);
        ASSERT_TRUE(ostm.append(std::string("second")));
        EXPECT_EQ(ostm.total(), 6u);
    }
    EXPECT_EQ(file::read_all(file_info(path)).to_string(), "second");

    std::remove(path.c_str());
}

TEST(gt_astream_ex, ostream_file_reset_append_mode_keeps_file) {
    const std::string path = "tmp-alxcore-gtest/alx_gt_ostream_reset_ap.bin";
    std::remove(path.c_str());
    ASSERT_TRUE(file::write_all(file_info(path), "base", 4, false));
    {
        ostream_file ostm(file_info(path), true);
        ASSERT_TRUE(ostm.append(std::string("+more")));
        ostm.reset();
        ASSERT_TRUE(ostm.append(std::string("+tail")));
    }
    EXPECT_EQ(file::read_all(file_info(path)).to_string(), "base+tail");

    std::remove(path.c_str());
}
