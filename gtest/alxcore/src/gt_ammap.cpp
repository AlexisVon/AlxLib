/*****************************************************************/ /**
 * \file   gt_ammap.cpp
 * \brief  mmap tests — offset/size bounds
 *
 * \author alexis
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#include "ajson.h"
#include "aplatform.h"
#include "astream.h"
#include <gtest/gtest.h>

using namespace alx;

TEST(gt_ammap, SetBounds) {
    alx::mmap m("alx_gt_mmap_bounds");
    ASSERT_TRUE(m.open(0x10000, true));

    EXPECT_TRUE(m.set(0xAA, 0xFFFF));
    EXPECT_TRUE(m.set(0xAA, 0, 0x10000));
    EXPECT_TRUE(m.set(0xAA, 0xFF00, 0x100));
    EXPECT_FALSE(m.set(0xAA, 0x10000));
    EXPECT_FALSE(m.set(0xAA, 0xFFFF, 2));
    EXPECT_TRUE(m.set(0xAA, 0, 0));

    EXPECT_FALSE(m.set(0xAA, 0xFFFFFF00, 0x200));
    EXPECT_FALSE(m.set(0xAA, 0xFFFFFFFF, 1));

    m.destroy();
}

TEST(gt_ammap, OwnerFlag) {
    alx::mmap creator("alx_gt_mmap_owner");
    creator.destroy();
    EXPECT_FALSE(creator.owner());
    ASSERT_TRUE(creator.open(0x1000, true));
    EXPECT_TRUE(creator.owner());

    alx::mmap attacher("alx_gt_mmap_owner");
    ASSERT_TRUE(attacher.open(0x1000, false));
    EXPECT_FALSE(attacher.owner());
}

#ifndef _WIN32
TEST(gt_ammap, AttachFromAnotherProcess) {
    alx::mmap creator("alx_gt_mmap_fork");
    creator.destroy();
    ASSERT_TRUE(creator.open(0x1000, true));

    const pid_t pid = fork();
    ASSERT_NE(-1, pid);
    if (0 == pid) {
        alx::mmap child("alx_gt_mmap_fork");
        const bool ok = child.open(0x1000, false) && !child.owner();
        child.close();
        _exit(ok ? 0 : 1);
    }
    int status = 0;
    ASSERT_EQ(pid, waitpid(pid, &status, 0));
    ASSERT_TRUE(WIFEXITED(status) && 0 == WEXITSTATUS(status));

    alx::mmap third("alx_gt_mmap_fork");
    EXPECT_TRUE(third.open(0x1000, false));
    third.close();
}
#endif

TEST(gt_ammap, AttachCloseKeepsObject) {
    alx::mmap creator("alx_gt_mmap_attach");
    creator.destroy();
    ASSERT_TRUE(creator.open(0x1000, true));

    {
        alx::mmap attacher("alx_gt_mmap_attach");
        ASSERT_TRUE(attacher.open(0x1000, false));
        attacher.close();
    }
    EXPECT_TRUE(creator.opened());

    alx::mmap third("alx_gt_mmap_attach");
    EXPECT_TRUE(third.open(0x1000, false));
    third.close();
}

TEST(gt_ammap, AttachSizeMismatch) {
    alx::mmap creator("alx_gt_mmap_size");
    creator.destroy();
    ASSERT_TRUE(creator.open(0x1000, true));

    alx::mmap too_big("alx_gt_mmap_size");
    EXPECT_FALSE(too_big.open(0x2000, false));
    alx::mmap smaller("alx_gt_mmap_size");
    EXPECT_TRUE(smaller.open(0x800, false));
}

TEST(gt_ammap, DestroyRemovesNames) {
    alx::mmap m("alx_gt_mmap_destroy");
    m.destroy();
    ASSERT_TRUE(m.open(0x1000, true));
    EXPECT_TRUE(m.destroy());
#ifndef _WIN32
    EXPECT_EQ(-1, ::shm_open("/alx_gt_mmap_destroy", O_RDWR, 0));
    EXPECT_EQ(SEM_FAILED, ::sem_open("/alx_gt_mmap_destroy_sem", 0));
#endif
    EXPECT_TRUE(m.destroy());
}

TEST(gt_ammap, DtorRemovesNamesForOwner) {
    {
        alx::mmap m("alx_gt_mmap_dtor");
        m.destroy();
        ASSERT_TRUE(m.open(0x1000, true));
    }
#ifndef _WIN32
    EXPECT_EQ(-1, ::shm_open("/alx_gt_mmap_dtor", O_RDWR, 0));
    EXPECT_EQ(SEM_FAILED, ::sem_open("/alx_gt_mmap_dtor_sem", 0));
#endif
}

TEST(gt_ammap, OwnerCloseKeepsNames) {
    {
        alx::mmap m("alx_gt_mmap_keep");
        m.destroy();
        ASSERT_TRUE(m.open(0x1000, true));
        m.close();
    }
    alx::mmap late("alx_gt_mmap_keep");
    EXPECT_TRUE(late.open(0x1000, false));
    late.close();

    alx::mmap cleaner("alx_gt_mmap_keep");
    EXPECT_TRUE(cleaner.destroy());
}

TEST(gt_ammap, ZeroLengthIsNoop) {
    alx::mmap m("alx_gt_mmap_zero");
    ASSERT_TRUE(m.open(0x1000, true));

    const char* src = "x";
    char buf[4] = {0};
    EXPECT_TRUE(m.write(src, 0, 0));
    EXPECT_TRUE(m.read(buf, 0, 0));
    EXPECT_TRUE(m.take(buf, 0, 0));
    EXPECT_TRUE(m.set(0xAA, 0, 0));
    EXPECT_TRUE(m.write(src, 0x1000, 0));
    EXPECT_EQ(0, buf[0]);

    m.destroy();
}

class mmap_ostream : public alx::ostream {
public:
    explicit mmap_ostream(alx::mmap& _map)
        : map_(_map) {}

    using ostream::append;
    bool append(const void* _ptr, alx::uint_64 _size) override {
        if (total_ + _size > map_.maxsize()) return false;
        const bool ok = map_.write(_ptr, (alx::uint_32) total_, (alx::uint_32) _size);
        if (ok) total_ += _size;
        return ok;
    }
    bool flush() override { return true; }
    void reset() override { total_ = 0; }
    alx::uint_64 total() const override { return total_; }

private:
    alx::mmap& map_;
    alx::uint_64 total_{0};
};

TEST(gt_ammap, JsonSinkOverMmap) {
    alx::mmap m("alx_gt_mmap_json_sink");
    ASSERT_TRUE(m.open(0x1000, true));

    alx::json_object doc;
    doc.insert("empty", alx::json_value(std::string()));
    doc.insert("quote\"first", alx::json_value(std::string("\"tail")));
    const std::string expect = alx::json_doc::to_json(doc, true);

    mmap_ostream sink(m);
    ASSERT_TRUE(alx::json_doc::to_json(std::move(doc), sink, true));
    alx::bytes got(expect.size());
    ASSERT_TRUE(m.read(got, 0));
    EXPECT_EQ(std::string((const char*) got.data(), (size_t) got.size()), expect);

    m.destroy();
}
