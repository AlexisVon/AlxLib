/*****************************************************************/ /**
 * \file   gt_afpacker.cpp
 * \brief  fpacker archive tests — block index validation and round trip
 *
 * \author alexis
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#include "afpacker.h"
#include "astream.h"
#include "avarsolid.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <gtest/gtest.h>

using namespace alx;

namespace {

    constexpr uint_32 ARC_HEAD = 0X5F415A4CU;
    constexpr uint_32 ARC_TAIL = 0X5F455942U;
    constexpr uint_64 ARC_HEAD_SIZE = 12;

    bytes make_archive_blocks(const varvec& _blocks) {
        varmap fsys;
        fsys.insert("blob", variant(_blocks));

        variant* err = nullptr;
        bytes fsys_data = varsolid::to_bytes(fsys, &err);
        if (nullptr != err) return bytes();

        bytes arc;
        ostream_buff os(arc);
        const uint_64 blob = fsys_data.size();
        os.append_ordinary(ARC_HEAD);
        os.append_ordinary(static_cast<uint_32>(0));
        os.append_ordinary(static_cast<uint_32>(0));
        os.append(fsys_data.data(), fsys_data.size());
        os.append_ordinary(blob);
        os.append_ordinary(blob);
        os.append_ordinary(ARC_TAIL);
        os.flush();
        return arc;
    }

    bytes make_archive(const std::vector<uint_64>& _mesgs) {
        varvec blocks;
        blocks.push_back(variant(_mesgs));
        blocks.push_back(variant(std::string("0123456789abcdef")));
        blocks.push_back(variant(static_cast<uint_64>(0)));
        return make_archive_blocks(blocks);
    }

    std::string pseudo_random(uint_64 _size, uint_32 _seed) {
        std::string out(_size, '\0');
        for (uint_64 i = 0; i < _size; ++i) {
            _seed = _seed * 1103515245U + 12345U;
            out[i] = static_cast<char>(_seed >> 16);
        }
        return out;
    }

    std::string write_temp(const std::string& _name, const std::string& _body) {
        const std::string path = "tmp-alxcore-gtest/" + _name;
        std::ofstream ofs(path, std::ios::binary);
        ofs.write(_body.data(), static_cast<std::streamsize>(_body.size()));
        return path;
    }

    bytes pack_files(const std::vector<std::string>& _paths, const std::string& _pswd, bool _cmps) {
        bytes out;
        fpacker::encoder enc(_cmps, _pswd);
        if (!enc.start(new ostream_buff(out))) return bytes();
        for (const std::string& path : _paths) {
            file_info info(path);
            if (enc.append(info, info.name()) != fpacker::encoder::success) return bytes();
        }
        return enc.finish() ? out : bytes();
    }

    uint_64 first_block_stored_size(const fpacker::decoder& _dec, const std::string& _key) {
        const varmap& fsys = _dec.fsystem();
        if (!fsys.contain(_key)) return 0;

        varvec blocks = fsys.value(_key).to<varvec>();
        if (blocks.empty()) return 0;

        std::vector<uint_64> mesgs = blocks[0].to<std::vector<uint_64>>();
        return mesgs.size() < 2 ? 0 : mesgs[1];
    }
}

TEST(gt_afpacker, RejectOneSlotBlockIndex) {

    bytes arc = make_archive({1});
    ASSERT_FALSE(arc.empty());

    fpacker::decoder dec(new istream_buff(bytes_view(arc)), "");
    ASSERT_TRUE(dec.valid());

    bytes out;
    EXPECT_EQ(dec.get("blob", std::list<std::string>(), out), fpacker::decoder::badblock);
}

TEST(gt_afpacker, RejectTwoSlotBlockIndex) {
    bytes arc = make_archive({1, 2});
    ASSERT_FALSE(arc.empty());

    fpacker::decoder dec(new istream_buff(bytes_view(arc)), "");
    ASSERT_TRUE(dec.valid());

    bytes out;
    EXPECT_EQ(dec.get("blob", std::list<std::string>(), out), fpacker::decoder::badblock);
}

TEST(gt_afpacker, RejectSingleSlotFileEntry) {

    varvec blocks;
    blocks.push_back(variant(varvec()));

    bytes arc = make_archive_blocks(blocks);
    ASSERT_FALSE(arc.empty());

    fpacker::decoder dec(new istream_buff(bytes_view(arc)), "");
    ASSERT_TRUE(dec.valid());

    bytes out;
    EXPECT_EQ(dec.get("blob", std::list<std::string>(), out), fpacker::decoder::badblock);
    EXPECT_TRUE(out.empty());
}

TEST(gt_afpacker, RoundTripFile) {
    const std::string path = "tmp-alxcore-gtest/alx_gt_afpacker_src.bin";
    const std::string body = "hello packer" + std::string(200, 'x');
    {
        std::ofstream ofs(path, std::ios::binary);
        ofs << body;
    }

    bytes packed;
    {
        fpacker::encoder enc(false, "");
        ASSERT_TRUE(enc.start(new ostream_buff(packed)));
        ASSERT_EQ(enc.append(file_info(path), "blob"), fpacker::encoder::success);
        ASSERT_TRUE(enc.finish());
    }
    ASSERT_FALSE(packed.empty());

    fpacker::decoder dec(new istream_buff(bytes_view(packed)), "");
    ASSERT_TRUE(dec.valid());

    bytes out;
    EXPECT_EQ(dec.get("blob", std::list<std::string>(), out), fpacker::decoder::success);
    EXPECT_EQ(out.size(), body.size());
    EXPECT_EQ(out.to_string(), body);

    std::remove(path.c_str());
}

TEST(gt_afpacker, RoundTripCompressedEncrypted) {

    const std::string body = "packer header|" + std::string(3000, 'a') + "|tail";
    const std::string path = write_temp("alx_gt_afp_full.bin", body);
    const bytes packed = pack_files({path}, "s3cret", true);
    ASSERT_FALSE(packed.empty());

    fpacker::decoder dec(new istream_buff(bytes_view(packed)), "s3cret");
    ASSERT_TRUE(dec.valid());
    EXPECT_TRUE(dec.is_encrypt());
    EXPECT_TRUE(dec.is_compress());

    bytes out;
    EXPECT_EQ(dec.get("alx_gt_afp_full.bin", std::list<std::string>(), out), fpacker::decoder::success);
    EXPECT_EQ(out.to_string(), body);
    std::remove(path.c_str());
}

TEST(gt_afpacker, WrongPasswordFails) {
    const std::string path = write_temp("alx_gt_afp_wrong.bin", std::string(2000, 'z'));
    const bytes packed = pack_files({path}, "right-pwd", true);
    ASSERT_FALSE(packed.empty());
    std::remove(path.c_str());

    fpacker::decoder dec(new istream_buff(bytes_view(packed)), "wrong-pwd");
    bool readable = false;
    if (dec.valid()) {
        bytes out;
        readable = (fpacker::decoder::success == dec.get("alx_gt_afp_wrong.bin", std::list<std::string>(), out));
    }
    EXPECT_FALSE(readable);
}

TEST(gt_afpacker, TamperDetected) {

    const std::string body = pseudo_random(1024, 0x2222);
    const std::string path = write_temp("alx_gt_afp_tamper.bin", body);

    bytes packed = pack_files({path}, "s3cret", true);
    ASSERT_GT(packed.size(), ARC_HEAD_SIZE + 8);
    bytes tampered = packed;
    tampered.data()[ARC_HEAD_SIZE + 8] ^= 0x01;

    fpacker::decoder dec(new istream_buff(bytes_view(tampered)), "s3cret");
    ASSERT_TRUE(dec.valid());
    bytes out;
    EXPECT_NE(dec.get("alx_gt_afp_tamper.bin", std::list<std::string>(), out), fpacker::decoder::success);
    EXPECT_TRUE(out.empty());

    bytes plain = pack_files({path}, "s3cret", false);
    ASSERT_GT(plain.size(), ARC_HEAD_SIZE + 8);
    plain.data()[ARC_HEAD_SIZE + 8] ^= 0x01;

    fpacker::decoder dec2(new istream_buff(bytes_view(plain)), "s3cret");
    ASSERT_TRUE(dec2.valid());
    bytes out2;
    EXPECT_EQ(dec2.get("alx_gt_afp_tamper.bin", std::list<std::string>(), out2), fpacker::decoder::badblock);
    EXPECT_TRUE(out2.empty());
    std::remove(path.c_str());

    const std::string rep_body = std::string(1024, 'q') + "end";
    const std::string rep_path = write_temp("alx_gt_afp_tamper_rep.bin", rep_body);
    const bytes rep = pack_files({rep_path}, "s3cret", true);
    ASSERT_GT(rep.size(), ARC_HEAD_SIZE + 16);
    for (uint_64 i = ARC_HEAD_SIZE; i < ARC_HEAD_SIZE + 16; ++i) {
        bytes flip = rep;
        flip.data()[i] ^= 0x01;

        fpacker::decoder d(new istream_buff(bytes_view(flip)), "s3cret");
        bytes o;
        if (fpacker::decoder::success == d.get("alx_gt_afp_tamper_rep.bin", std::list<std::string>(), o))
            EXPECT_EQ(o.to_string(), rep_body) << "flip at " << i << " changed the content";
    }
    std::remove(rep_path.c_str());
}

TEST(gt_afpacker, SameKeySameLayoutIsByteIdentical) {

    const std::string path = write_temp("alx_gt_afp_same.bin", std::string(500, 'k'));
    const bytes a = pack_files({path}, "same-pwd", true);
    const bytes b = pack_files({path}, "same-pwd", true);
    ASSERT_FALSE(a.empty());
    ASSERT_EQ(a.size(), b.size());
    EXPECT_EQ(0, std::memcmp(a.data(), b.data(), a.size()));

    const bytes c = pack_files({path}, "other-pwd", true);
    ASSERT_EQ(a.size(), c.size());
    EXPECT_NE(0, std::memcmp(a.data(), c.data(), a.size()));
    std::remove(path.c_str());
}

TEST(gt_afpacker, PerFileCompressionIndependent) {

    const std::string a = pseudo_random(64 * 1024, 0x1111);
    const std::string pa = write_temp("alx_gt_afp_a.bin", a);
    const std::string pb = write_temp("alx_gt_afp_b.bin", a.substr(a.size() - 4096));

    const bytes both = pack_files({pa, pb}, "pwd", true);
    const bytes only = pack_files({pb}, "pwd", true);
    ASSERT_FALSE(both.empty());
    ASSERT_FALSE(only.empty());

    fpacker::decoder d1(new istream_buff(bytes_view(both)), "pwd");
    fpacker::decoder d2(new istream_buff(bytes_view(only)), "pwd");
    ASSERT_TRUE(d1.valid());
    ASSERT_TRUE(d2.valid());

    const uint_64 s1 = first_block_stored_size(d1, "alx_gt_afp_b.bin");
    const uint_64 s2 = first_block_stored_size(d2, "alx_gt_afp_b.bin");
    EXPECT_GT(s1, 1000);
    EXPECT_EQ(s1, s2);
    std::remove(pa.c_str());
    std::remove(pb.c_str());
}
