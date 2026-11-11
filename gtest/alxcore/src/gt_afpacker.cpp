/*****************************************************************/ /**
 * \file   gt_afpacker.cpp
 * \brief  fpacker archive tests — block index validation, factory dispatch and round trip
 *
 * \author alexis
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#include "afpacker.h"
#include "astream.h"
#include "avarsolid.h"
#include "averify.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <gtest/gtest.h>
#include <memory>

using namespace alx;

namespace {

    constexpr uint_32 ARC_HEAD = 0X5F415A4CU;
    constexpr uint_32 ARC_CMPS = 0X5F535043U;
    constexpr uint_32 ARC_TAIL = 0X5F455942U;
    constexpr uint_64 ARC_HEAD_SIZE = 12;

    using dec_ptr = std::unique_ptr<fpacker::decoder>;
    using enc_ptr = std::unique_ptr<fpacker::encoder>;

    dec_ptr open_decoder(const bytes& _arc, const std::string& _pswd) {
        return dec_ptr(fpacker::decoder::create(new istream_buff(bytes_view(_arc)), _pswd));
    }

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

    bytes pack_files(const std::vector<std::string>& _paths, const std::string& _pswd, bool _cmps,
                     uint_64 _blck_size = 0X04000000U, bool _v2 = false, uint_32 _iterations = 0X00400000U) {
        bytes out;
        enc_ptr enc;
        if (_v2) enc.reset(fpacker::encoder::create_encrypt_v2(_pswd, _cmps, nullptr, _blck_size, _iterations));
        else if (_pswd.empty()) enc.reset(fpacker::encoder::create(_cmps, nullptr, _blck_size));
        else enc.reset(fpacker::encoder::create_encrypt_v1(_pswd, _cmps, nullptr, _blck_size));
        if (!enc) return bytes();
        if (!enc->start(new ostream_buff(out))) return bytes();
        for (const std::string& path : _paths) {
            file_info info(path);
            if (enc->append(info, info.name()) != fpacker::encoder::success) return bytes();
        }
        return enc->finish() ? out : bytes();
    }

    uint_64 first_block_stored_size(const fpacker::decoder& _dec, const std::string& _key) {
        const varmap& fsys = _dec.fsystem();
        if (!fsys.contain(_key)) return 0;

        varvec blocks = fsys.value(_key).to<varvec>();
        if (blocks.empty()) return 0;

        std::vector<uint_64> mesgs = blocks[0].to<std::vector<uint_64>>();
        return mesgs.size() < 2 ? 0 : mesgs[1];
    }

    void expect_pktype(const bytes& _arc, bool _clan, bool _cmps, bool _encrypt, uint_8 _version) {
        istream_buff is{bytes_view(_arc)};
        const fpacker::decoder::pktype type = fpacker::decoder::probe(is);
        EXPECT_EQ(_clan, type.clan);
        EXPECT_EQ(_cmps, type.cmps);
        EXPECT_EQ(_encrypt, type.encrypt);
        EXPECT_EQ(_version, type.version);
        EXPECT_EQ(!_clan, 0 == type.valid());
    }
}

TEST(gt_afpacker, RejectOneSlotBlockIndex) {

    bytes arc = make_archive({1});
    ASSERT_FALSE(arc.empty());

    dec_ptr dec = open_decoder(arc, "");
    ASSERT_TRUE(dec);

    bytes out;
    EXPECT_EQ(dec->get("blob", std::list<std::string>(), out), fpacker::decoder::badblock);
}

TEST(gt_afpacker, RejectTwoSlotBlockIndex) {
    bytes arc = make_archive({1, 2});
    ASSERT_FALSE(arc.empty());

    dec_ptr dec = open_decoder(arc, "");
    ASSERT_TRUE(dec);

    bytes out;
    EXPECT_EQ(dec->get("blob", std::list<std::string>(), out), fpacker::decoder::badblock);
}

TEST(gt_afpacker, RejectSingleSlotFileEntry) {

    varvec blocks;
    blocks.push_back(variant(varvec()));

    bytes arc = make_archive_blocks(blocks);
    ASSERT_FALSE(arc.empty());

    dec_ptr dec = open_decoder(arc, "");
    ASSERT_TRUE(dec);

    bytes out;
    EXPECT_EQ(dec->get("blob", std::list<std::string>(), out), fpacker::decoder::badblock);
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
        enc_ptr enc(fpacker::encoder::create(false));
        ASSERT_TRUE(enc);
        ASSERT_TRUE(enc->start(new ostream_buff(packed)));
        ASSERT_EQ(enc->append(file_info(path), "blob"), fpacker::encoder::success);
        ASSERT_TRUE(enc->finish());
    }
    ASSERT_FALSE(packed.empty());

    dec_ptr dec = open_decoder(packed, "");
    ASSERT_TRUE(dec);

    bytes out;
    EXPECT_EQ(dec->get("blob", std::list<std::string>(), out), fpacker::decoder::success);
    EXPECT_EQ(out.size(), body.size());
    EXPECT_EQ(out.to_string(), body);

    std::remove(path.c_str());
}

TEST(gt_afpacker, RoundTripCompressedEncrypted) {

    const std::string body = "packer header|" + std::string(3000, 'a') + "|tail";
    const std::string path = write_temp("alx_gt_afp_full.bin", body);
    const bytes packed = pack_files({path}, "s3cret", true);
    ASSERT_FALSE(packed.empty());

    dec_ptr dec = open_decoder(packed, "s3cret");
    ASSERT_TRUE(dec);
    EXPECT_TRUE(dec->is_encrypt());
    EXPECT_TRUE(dec->is_compress());

    bytes out;
    EXPECT_EQ(dec->get("alx_gt_afp_full.bin", std::list<std::string>(), out), fpacker::decoder::success);
    EXPECT_EQ(out.to_string(), body);
    std::remove(path.c_str());
}

TEST(gt_afpacker, WrongPasswordFails) {
    const std::string path = write_temp("alx_gt_afp_wrong.bin", std::string(2000, 'z'));
    const bytes packed = pack_files({path}, "right-pwd", true);
    ASSERT_FALSE(packed.empty());
    std::remove(path.c_str());

    uint_8 err = fpacker::decoder::success;
    dec_ptr dec(fpacker::decoder::create(new istream_buff(bytes_view(packed)), "wrong-pwd", &err));
    EXPECT_FALSE(dec);
    EXPECT_NE(fpacker::decoder::success, err);
    EXPECT_TRUE(fpacker::decoder::dcmpsfail == err || fpacker::decoder::badfsys == err);
}

TEST(gt_afpacker, TamperDetected) {

    const std::string body = pseudo_random(1024, 0x2222);
    const std::string path = write_temp("alx_gt_afp_tamper.bin", body);

    bytes packed = pack_files({path}, "s3cret", true);
    ASSERT_GT(packed.size(), ARC_HEAD_SIZE + 8);
    bytes tampered = packed;
    tampered.data()[ARC_HEAD_SIZE + 8] ^= 0x01;

    dec_ptr dec = open_decoder(tampered, "s3cret");
    ASSERT_TRUE(dec);
    bytes out;
    EXPECT_NE(dec->get("alx_gt_afp_tamper.bin", std::list<std::string>(), out), fpacker::decoder::success);
    EXPECT_TRUE(out.empty());

    bytes plain = pack_files({path}, "s3cret", false);
    ASSERT_GT(plain.size(), ARC_HEAD_SIZE + 8);
    plain.data()[ARC_HEAD_SIZE + 8] ^= 0x01;

    dec_ptr dec2 = open_decoder(plain, "s3cret");
    ASSERT_TRUE(dec2);
    bytes out2;
    EXPECT_EQ(dec2->get("alx_gt_afp_tamper.bin", std::list<std::string>(), out2), fpacker::decoder::badblock);
    EXPECT_TRUE(out2.empty());
    std::remove(path.c_str());

    const std::string rep_body = std::string(1024, 'q') + "end";
    const std::string rep_path = write_temp("alx_gt_afp_tamper_rep.bin", rep_body);
    const bytes rep = pack_files({rep_path}, "s3cret", true);
    ASSERT_GT(rep.size(), ARC_HEAD_SIZE + 16);
    for (uint_64 i = ARC_HEAD_SIZE; i < ARC_HEAD_SIZE + 16; ++i) {
        bytes flip = rep;
        flip.data()[i] ^= 0x01;

        dec_ptr d = open_decoder(flip, "s3cret");
        if (!d) continue;
        bytes o;
        if (fpacker::decoder::success == d->get("alx_gt_afp_tamper_rep.bin", std::list<std::string>(), o))
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

TEST(gt_afpacker, V1OutputIsStable) {
    const std::string path = write_temp("alx_gt_afp_stable.bin", "stable payload" + std::string(500, 's'));
    const bytes packed = pack_files({path}, "stable-pwd", true);
    std::remove(path.c_str());
    ASSERT_FALSE(packed.empty());
    // v1 writes host byte order: this digest pins the little-endian targets against later drift
    EXPECT_EQ("353281a7deceb71da077b3b1acbbbabd5cae7182d2b9d96dee81dfa3506672c4",
              verify::exec(verify::SHA_256, packed));
}

TEST(gt_afpacker, PerFileCompressionIndependent) {

    const std::string a = pseudo_random(64 * 1024, 0x1111);
    const std::string pa = write_temp("alx_gt_afp_a.bin", a);
    const std::string pb = write_temp("alx_gt_afp_b.bin", a.substr(a.size() - 4096));

    const bytes both = pack_files({pa, pb}, "pwd", true);
    const bytes only = pack_files({pb}, "pwd", true);
    ASSERT_FALSE(both.empty());
    ASSERT_FALSE(only.empty());

    dec_ptr d1 = open_decoder(both, "pwd");
    dec_ptr d2 = open_decoder(only, "pwd");
    ASSERT_TRUE(d1);
    ASSERT_TRUE(d2);

    const uint_64 s1 = first_block_stored_size(*d1, "alx_gt_afp_b.bin");
    const uint_64 s2 = first_block_stored_size(*d2, "alx_gt_afp_b.bin");
    EXPECT_GT(s1, 1000);
    EXPECT_EQ(s1, s2);
    std::remove(pa.c_str());
    std::remove(pb.c_str());
}

TEST(gt_afpacker, CreateRejectsUnknownMagic) {
    const bytes junk(std::string("nope"));
    uint_8 err = fpacker::decoder::success;
    dec_ptr dec(fpacker::decoder::create(new istream_buff(bytes_view(junk)), "", &err));
    EXPECT_FALSE(dec);
    EXPECT_EQ(fpacker::decoder::badversion, err);
}

TEST(gt_afpacker, CreateRejectsBadHeadFlag) {
    bytes arc = make_archive({1});
    ASSERT_FALSE(arc.empty());
    arc.data()[4] = 0XFF; // the compression flag word no longer names a known value

    uint_8 err = fpacker::decoder::success;
    dec_ptr dec(fpacker::decoder::create(new istream_buff(bytes_view(arc)), "", &err));
    EXPECT_FALSE(dec);
    EXPECT_EQ(fpacker::decoder::badversion, err);
}

TEST(gt_afpacker, CreateRejectsShortStream) {
    uint_8 err = fpacker::decoder::success;
    dec_ptr dec(fpacker::decoder::create(new istream_buff(bytes_view()), "", &err));
    EXPECT_FALSE(dec);
    EXPECT_EQ(fpacker::decoder::badversion, err);
}

TEST(gt_afpacker, CreateReportsSuccess) {
    const std::string path = write_temp("alx_gt_afp_ok.bin", "ok");
    const bytes packed = pack_files({path}, "", false);
    ASSERT_FALSE(packed.empty());
    std::remove(path.c_str());

    uint_8 err = fpacker::decoder::success;
    dec_ptr dec(fpacker::decoder::create(new istream_buff(bytes_view(packed)), "", &err));
    ASSERT_TRUE(dec);
    EXPECT_EQ(fpacker::decoder::success, err);
}

TEST(gt_afpacker, EncoderFactoryRejectsBadArguments) {
    uint_8 err = fpacker::encoder::success;

    enc_ptr a(fpacker::encoder::create(false, &err, 0));
    EXPECT_FALSE(a);
    EXPECT_EQ(fpacker::encoder::badparams, err);

    err = fpacker::encoder::success;
    enc_ptr b(fpacker::encoder::create_encrypt_v1("", false, &err));
    EXPECT_FALSE(b);
    EXPECT_EQ(fpacker::encoder::badparams, err);

    err = fpacker::encoder::success;
    enc_ptr c(fpacker::encoder::create_encrypt_v1("pwd", false, &err, 0));
    EXPECT_FALSE(c);
    EXPECT_EQ(fpacker::encoder::badparams, err);

    err = fpacker::encoder::success;
    enc_ptr d(fpacker::encoder::create_encrypt_v2("", false, &err));
    EXPECT_FALSE(d);
    EXPECT_EQ(fpacker::encoder::badparams, err);

    err = fpacker::encoder::success;
    enc_ptr e(fpacker::encoder::create_encrypt_v2("pwd", false, &err, 0X0000FFFFU));
    EXPECT_FALSE(e);
    EXPECT_EQ(fpacker::encoder::badparams, err);

    err = fpacker::encoder::success;
    enc_ptr f(fpacker::encoder::create_encrypt_v2("pwd", false, &err, 0X04000000U, 0));
    EXPECT_FALSE(f);
    EXPECT_EQ(fpacker::encoder::badparams, err);

    err = fpacker::encoder::success;
    enc_ptr g(fpacker::encoder::create_encrypt_v2("pwd", false, &err, 0X04000000U, 0X01000001U));
    EXPECT_FALSE(g);
    EXPECT_EQ(fpacker::encoder::badparams, err);

    err = fpacker::encoder::success;
    enc_ptr ok(fpacker::encoder::create_encrypt_v2("pwd", false, &err));
    EXPECT_TRUE(ok);
    EXPECT_EQ(fpacker::encoder::success, err);
}

TEST(gt_afpacker, V1IgnoresArchiveMetadata) {
    const std::string path = write_temp("alx_gt_afp_meta.bin", "meta");

    bytes packed;
    {
        enc_ptr enc(fpacker::encoder::create(false));
        ASSERT_TRUE(enc);
        ASSERT_TRUE(enc->start(new ostream_buff(packed)));
        enc->archive().insert("note", variant(std::string("ignored")));
        ASSERT_EQ(enc->append(file_info(path), "alx_gt_afp_meta.bin"), fpacker::encoder::success);
        ASSERT_TRUE(enc->finish());
    }
    ASSERT_FALSE(packed.empty());
    std::remove(path.c_str());

    dec_ptr dec = open_decoder(packed, "");
    ASSERT_TRUE(dec);
    EXPECT_TRUE(dec->archive().empty());
}

TEST(gt_afpacker, DestructorFinishesOpenArchive) {
    const std::string path = write_temp("alx_gt_afp_dtor.bin", "dtor");

    bytes packed;
    {
        enc_ptr enc(fpacker::encoder::create(false));
        ASSERT_TRUE(enc);
        ASSERT_TRUE(enc->start(new ostream_buff(packed)));
        ASSERT_EQ(enc->append(file_info(path), "alx_gt_afp_dtor.bin"), fpacker::encoder::success);
        // no finish(): the destructor closes the archive
    }
    std::remove(path.c_str());
    ASSERT_FALSE(packed.empty());

    dec_ptr dec = open_decoder(packed, "");
    ASSERT_TRUE(dec);
    bytes out;
    EXPECT_EQ(dec->get("alx_gt_afp_dtor.bin", std::list<std::string>(), out), fpacker::decoder::success);
    EXPECT_EQ(out.to_string(), "dtor");
}

TEST(gt_afpacker, EmptyArchiveIsRefused) {
    bytes packed;
    enc_ptr enc(fpacker::encoder::create(true));
    ASSERT_TRUE(enc);
    ASSERT_TRUE(enc->start(new ostream_buff(packed)));
    EXPECT_FALSE(enc->finish());
}

TEST(gt_afpacker, BlockSizeCapIsExact) {
    constexpr uint_64 at_cap = 0XFFFFFFFFU;
    constexpr uint_64 over_cap = 0X100000000ULL;
    uint_8 err = fpacker::encoder::success;

    // the cap is inclusive: the largest accepted value still builds
    enc_ptr ok0(fpacker::encoder::create(false, &err, at_cap));
    EXPECT_NE(nullptr, ok0.get());
    enc_ptr ok1(fpacker::encoder::create_encrypt_v1("pwd", false, &err, at_cap));
    EXPECT_NE(nullptr, ok1.get());
    enc_ptr ok2(fpacker::encoder::create_encrypt_v2("pwd", false, &err, at_cap));
    EXPECT_NE(nullptr, ok2.get());

    // one byte above is refused by every factory
    EXPECT_FALSE(fpacker::encoder::create(false, &err, over_cap));
    EXPECT_EQ(fpacker::encoder::badparams, err);

    err = fpacker::encoder::success;
    EXPECT_FALSE(fpacker::encoder::create_encrypt_v1("pwd", false, &err, over_cap));
    EXPECT_EQ(fpacker::encoder::badparams, err);

    err = fpacker::encoder::success;
    EXPECT_FALSE(fpacker::encoder::create_encrypt_v2("pwd", false, &err, over_cap));
    EXPECT_EQ(fpacker::encoder::badparams, err);
}

TEST(gt_afpacker, ProbeClassifiesHeaders) {
    // shorter than the common head, and a plainly foreign stream: no family claims either
    expect_pktype(bytes("short"), false, false, false, 0);
    expect_pktype(bytes("0123456789abcdef"), false, false, false, 0);

    // the common head alone is not enough: an unknown cipher word belongs to no family
    bytes head_only;
    {
        ostream_buff os(head_only);
        os.append_ordinary(ARC_HEAD);
        os.append_ordinary(ARC_CMPS);
        os.append_ordinary(static_cast<uint_32>(0XDEADBEEFU));
        os.flush();
    }
    expect_pktype(head_only, false, false, false, 0);

    const std::string path = write_temp("alx_gt_afp_probe.bin", pseudo_random(70000, 0X9E37));
    const bytes v1_plain = pack_files({path}, "", false);
    const bytes v1_plain_cmps = pack_files({path}, "", true);
    const bytes v1_sealed = pack_files({path}, "probe-pwd", false);
    const bytes v1_sealed_cmps = pack_files({path}, "probe-pwd", true);
    const bytes v2 = pack_files({path}, "probe-pwd", false, 0X04000000U, true, 0X000186A0U);
    const bytes v2_cmps = pack_files({path}, "probe-pwd", true, 0X04000000U, true, 0X000186A0U);
    std::remove(path.c_str());

    ASSERT_FALSE(v1_plain.empty());
    ASSERT_FALSE(v1_plain_cmps.empty());
    ASSERT_FALSE(v1_sealed.empty());
    ASSERT_FALSE(v1_sealed_cmps.empty());
    ASSERT_FALSE(v2.empty());
    ASSERT_FALSE(v2_cmps.empty());

    expect_pktype(v1_plain, true, false, false, 1);
    expect_pktype(v1_plain_cmps, true, true, false, 1);
    expect_pktype(v1_sealed, true, false, true, 1);
    expect_pktype(v1_sealed_cmps, true, true, true, 1);
    expect_pktype(v2, true, false, true, 2);
    expect_pktype(v2_cmps, true, true, true, 2);
}

TEST(gt_afpacker, V2RoundTrip) {
    const std::string body = pseudo_random(300 * 1024, 0X5151);
    const std::string path = write_temp("alx_gt_afp_v2.bin", body);

    const bytes packed = pack_files({path}, "v2-pwd", true, 0X00010000U, true, 0X000186A0U);
    const bytes plain = pack_files({path}, "v2-pwd", false, 0X00010000U, true, 0X000186A0U);
    std::remove(path.c_str());
    ASSERT_FALSE(packed.empty());
    ASSERT_FALSE(plain.empty());

    for (const bytes& arc : {packed, plain}) {
        dec_ptr dec = open_decoder(arc, "v2-pwd");
        ASSERT_TRUE(dec);
        EXPECT_TRUE(dec->is_encrypt());
        bytes out;
        EXPECT_EQ(fpacker::decoder::success, dec->get("alx_gt_afp_v2.bin", std::list<std::string>(), out));
        EXPECT_EQ(body, out.to_string());
    }

    dec_ptr cd = open_decoder(packed, "v2-pwd");
    ASSERT_TRUE(cd);
    EXPECT_TRUE(cd->is_compress());
    dec_ptr ud = open_decoder(plain, "v2-pwd");
    ASSERT_TRUE(ud);
    EXPECT_FALSE(ud->is_compress());
}

TEST(gt_afpacker, V2WrongPasswordIsLoud) {
    const std::string path = write_temp("alx_gt_afp_v2wrong.bin", std::string(2000, 'z'));
    const bytes packed = pack_files({path}, "right-pwd", true, 0X04000000U, true, 0X000186A0U);
    std::remove(path.c_str());
    ASSERT_FALSE(packed.empty());

    uint_8 err = fpacker::decoder::success;
    dec_ptr dec(fpacker::decoder::create(new istream_buff(bytes_view(packed)), "wrong-pwd", &err));
    EXPECT_FALSE(dec);
    EXPECT_EQ(fpacker::decoder::badauth, err);
}

TEST(gt_afpacker, V2MetadataRoundTrips) {
    const std::string path = write_temp("alx_gt_afp_v2meta.bin", "meta");

    bytes packed;
    {
        uint_8 err = fpacker::encoder::success;
        enc_ptr enc(fpacker::encoder::create_encrypt_v2("pwd", false, &err, 0X04000000U, 0X000186A0U));
        ASSERT_TRUE(enc);
        ASSERT_EQ(fpacker::encoder::success, err);
        ASSERT_TRUE(enc->start(new ostream_buff(packed)));
        enc->archive().insert("created_at", variant((uint_64) 1234));
        ASSERT_EQ(fpacker::encoder::success, enc->append(file_info(path), "alx_gt_afp_v2meta.bin"));
        ASSERT_TRUE(enc->finish());
    }
    std::remove(path.c_str());
    ASSERT_FALSE(packed.empty());

    dec_ptr dec = open_decoder(packed, "pwd");
    ASSERT_TRUE(dec);
    ASSERT_TRUE(dec->archive().contain("created_at"));
    EXPECT_EQ((uint_64) 1234, dec->archive().value("created_at").to<uint_64>());

    bytes out;
    EXPECT_EQ(fpacker::decoder::success, dec->get("alx_gt_afp_v2meta.bin", std::list<std::string>(), out));
    EXPECT_EQ("meta", out.to_string());
}

TEST(gt_afpacker, EmptyFileRoundTrips) {
    const std::string path = write_temp("alx_gt_afp_empty.bin", "");
    const bytes v1 = pack_files({path}, "", false);
    const bytes v2 = pack_files({path}, "pwd", false, 0X04000000U, true, 0X000186A0U);
    std::remove(path.c_str());
    ASSERT_FALSE(v1.empty());
    ASSERT_FALSE(v2.empty());

    dec_ptr d1 = open_decoder(v1, "");
    ASSERT_TRUE(d1);
    bytes o1;
    EXPECT_EQ(fpacker::decoder::success, d1->get("alx_gt_afp_empty.bin", std::list<std::string>(), o1));
    EXPECT_TRUE(o1.empty());

    dec_ptr d2 = open_decoder(v2, "pwd");
    ASSERT_TRUE(d2);
    bytes o2;
    EXPECT_EQ(fpacker::decoder::success, d2->get("alx_gt_afp_empty.bin", std::list<std::string>(), o2));
    EXPECT_TRUE(o2.empty());
}

TEST(gt_afpacker, V2TamperIsLoud) {
    const std::string path = write_temp("alx_gt_afp_v2tamper.bin", pseudo_random(4096, 0X7777));
    const bytes packed = pack_files({path}, "pwd", false, 0X04000000U, true, 0X000186A0U);
    std::remove(path.c_str());
    ASSERT_GT(packed.size(), 64U);

    uint_8 err = fpacker::decoder::success;
    bytes bad_index = packed;
    bad_index.data()[packed.size() - 25] ^= 0X01;
    dec_ptr d0(fpacker::decoder::create(new istream_buff(bytes_view(bad_index)), "pwd", &err));
    EXPECT_FALSE(d0);
    EXPECT_EQ(fpacker::decoder::badauth, err);

    bytes bad_block = packed;
    bad_block.data()[33] ^= 0X01;
    dec_ptr d1 = open_decoder(bad_block, "pwd");
    ASSERT_TRUE(d1);
    bytes out;
    EXPECT_EQ(fpacker::decoder::badauth, d1->get("alx_gt_afp_v2tamper.bin", std::list<std::string>(), out));

    bytes bad_salt = packed;
    bad_salt.data()[17] ^= 0X01;
    err = fpacker::decoder::success;
    dec_ptr d2(fpacker::decoder::create(new istream_buff(bytes_view(bad_salt)), "pwd", &err));
    EXPECT_FALSE(d2);
    EXPECT_EQ(fpacker::decoder::badauth, err);
}

TEST(gt_afpacker, V2TruncationIsLoud) {
    const std::string path = write_temp("alx_gt_afp_v2trunc.bin", pseudo_random(4096, 0X1234));
    const bytes packed = pack_files({path}, "pwd", false, 0X04000000U, true, 0X000186A0U);
    std::remove(path.c_str());
    ASSERT_GT(packed.size(), 64U);

    bytes cut = packed.mid(0, packed.size() / 2);
    uint_8 err = fpacker::decoder::success;
    dec_ptr dec(fpacker::decoder::create(new istream_buff(bytes_view(cut)), "pwd", &err));
    EXPECT_FALSE(dec);
    EXPECT_EQ(fpacker::decoder::badversion, err);
}

TEST(gt_afpacker, V2TrailerTamperIsLoud) {
    const std::string path = write_temp("alx_gt_afp_v2trail.bin", pseudo_random(4096, 0X5678));
    const bytes packed = pack_files({path}, "pwd", false, 0X04000000U, true, 0X000186A0U);
    std::remove(path.c_str());
    ASSERT_GT(packed.size(), 64U);

    // the tail magic is the cheapest completeness check
    bytes bad_tail = packed;
    bad_tail.data()[packed.size() - 1] ^= 0X01;
    uint_8 err = fpacker::decoder::success;
    dec_ptr d0(fpacker::decoder::create(new istream_buff(bytes_view(bad_tail)), "pwd", &err));
    EXPECT_FALSE(d0);
    EXPECT_EQ(fpacker::decoder::badversion, err);

    // the index sizes live in the untagged trailer; the index AAD binds them
    bytes bad_plain = packed;
    bad_plain.data()[packed.size() - 20] ^= 0X01;
    err = fpacker::decoder::success;
    dec_ptr d1(fpacker::decoder::create(new istream_buff(bytes_view(bad_plain)), "pwd", &err));
    EXPECT_FALSE(d1);
    EXPECT_EQ(fpacker::decoder::badauth, err);
}

TEST(gt_afpacker, V2BlockRelocationIsLoud) {
    const std::string path = write_temp("alx_gt_afp_v2move.bin", pseudo_random(2 * 0X00010000U, 0X9ABC));
    const bytes packed = pack_files({path}, "pwd", false, 0X00010000U, true, 0X000186A0U);
    std::remove(path.c_str());
    ASSERT_FALSE(packed.empty());

    dec_ptr dec = open_decoder(packed, "pwd");
    ASSERT_TRUE(dec);
    varvec blocks = dec->fsystem().value("alx_gt_afp_v2move.bin").to<varvec>();
    ASSERT_EQ(3U, blocks.size());
    std::vector<uint_64> b0 = blocks[0].to<std::vector<uint_64>>();
    std::vector<uint_64> b1 = blocks[1].to<std::vector<uint_64>>();
    ASSERT_EQ(3U, b0.size());
    ASSERT_EQ(b0[1], b1[1]);
    dec.reset();

    // block 1's ciphertext moved into block 0's slot: the nonce and the AAD still name slot 0
    bytes moved = packed;
    std::memcpy(moved.data() + b0[2], packed.data() + b1[2], static_cast<size_t>(b1[1]));

    dec_ptr bad = open_decoder(moved, "pwd");
    ASSERT_TRUE(bad);
    bytes out;
    EXPECT_EQ(fpacker::decoder::badauth, bad->get("alx_gt_afp_v2move.bin", std::list<std::string>(), out));
}

TEST(gt_afpacker, V2HeadParametersAreChecked) {
    const std::string path = write_temp("alx_gt_afp_v2head.bin", "head");
    const bytes packed = pack_files({path}, "pwd", false, 0X04000000U, true, 0X000186A0U);
    std::remove(path.c_str());
    ASSERT_GT(packed.size(), 33U);

    uint_8 err = fpacker::decoder::success;
    bytes bad_kdf = packed;
    bad_kdf.data()[12] = 0;
    dec_ptr d0(fpacker::decoder::create(new istream_buff(bytes_view(bad_kdf)), "pwd", &err));
    EXPECT_FALSE(d0);
    EXPECT_EQ(fpacker::decoder::badparams, err);

    bytes zero_iter = packed;
    for (int i = 13; i <= 16; ++i) zero_iter.data()[i] = 0;
    err = fpacker::decoder::success;
    dec_ptr d1(fpacker::decoder::create(new istream_buff(bytes_view(zero_iter)), "pwd", &err));
    EXPECT_FALSE(d1);
    EXPECT_EQ(fpacker::decoder::badparams, err);

    bytes big_iter = packed;
    big_iter.data()[13] = 0X01;
    big_iter.data()[14] = 0;
    big_iter.data()[15] = 0;
    big_iter.data()[16] = 0X01;
    err = fpacker::decoder::success;
    dec_ptr d2(fpacker::decoder::create(new istream_buff(bytes_view(big_iter)), "pwd", &err));
    EXPECT_FALSE(d2);
    EXPECT_EQ(fpacker::decoder::badparams, err);
}

TEST(gt_afpacker, V2SaltIsFresh) {
    const std::string path = write_temp("alx_gt_afp_v2salt.bin", std::string(500, 'k'));
    const bytes a = pack_files({path}, "pwd", true, 0X04000000U, true, 0X000186A0U);
    const bytes b = pack_files({path}, "pwd", true, 0X04000000U, true, 0X000186A0U);
    std::remove(path.c_str());
    ASSERT_FALSE(a.empty());
    ASSERT_EQ(a.size(), b.size());
    EXPECT_NE(0, std::memcmp(a.data(), b.data(), a.size()));
}

TEST(gt_afpacker, V2CompressedMultiBlockKeepsLz4Chain) {
    const std::string first = pseudo_random(64 * 1024, 0X3131);
    // the second block opens with a copy of the first block's tail, so LZ4 must reach across
    const std::string body = first + first.substr(first.size() - 32 * 1024) + pseudo_random(32 * 1024, 0X3232);
    const std::string path = write_temp("alx_gt_afp_v2chain.bin", body);
    const bytes packed = pack_files({path}, "pwd", true, 0X00010000U, true, 0X000186A0U);
    std::remove(path.c_str());
    ASSERT_FALSE(packed.empty());

    dec_ptr dec = open_decoder(packed, "pwd");
    ASSERT_TRUE(dec);
    bytes out;
    EXPECT_EQ(fpacker::decoder::success, dec->get("alx_gt_afp_v2chain.bin", std::list<std::string>(), out));
    EXPECT_EQ(body, out.to_string());
}
