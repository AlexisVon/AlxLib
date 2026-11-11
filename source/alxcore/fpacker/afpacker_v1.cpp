/*****************************************************************/ /**
 * \file   afpacker_v1.cpp
 * \brief  The v1 archive envelope — AES-256-CTR, bare tree index, 12-byte header
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "afpacker_v1.h"

#include "aaes.h"
#include "autility.h"
#include "avarsolid.h"
#include "averify.h"

using namespace alx;
using namespace alx::fpacker;
using namespace alx::fpacker::detail;

namespace {

    constexpr verify::COMMON_TYPE CHECK_TYPE = verify::CRC_32C;

    bytes make_key(const std::string& _pswd) {
        return _pswd.empty() ? bytes() : bytes::from_hex(verify::exec(verify::SHA_256, _pswd));
    }
}

encoder_v1::encoder_v1(bool _cmps, uint_64 _blck_size, const std::string& _pswd)
    : encoder_base(_cmps, _blck_size), key_(make_key(_pswd)) {
}

bool encoder_v1::write_head(ostream* _ostm) { return _ostm->append_ordinary(key_.empty() ? ARC_NONE : ARC_AESC); }

uint_8 encoder_v1::write_block(const bytes& _plain, uint_64 _offset, bytes& _stored, varvec& _blocks) {
    // one block is two index entries: {plain size, stored size, archive offset} then the CRC of the plain bytes
    std::vector<uint_64> block_mesgs;
    block_mesgs.push_back(_plain.size());
    const std::string block_check = verify::exec(CHECK_TYPE, _plain);

    _stored = _plain;
    if (!compress_block(_stored)) return encoder::cmpsfail;
    block_mesgs.push_back(_stored.size());
    block_mesgs.push_back(_offset);

    if (!key_.empty()) {
        const uint_64 iv[2]{block_mesgs[1], block_mesgs[2]};
        aes_ctr<256>(key_.data(), (const uint_8*) iv).encrypt(_stored);
    }

    _blocks.push_back(block_mesgs);
    _blocks.push_back(block_check);
    return encoder::success;
}

bool encoder_v1::write_meta(ostream* _ostm, const varmap& _archive, const varmap& _tree) {
    (void) _archive;
    variant* error{nullptr};
    bytes data = varsolid::to_bytes(_tree, &error);
    if (nullptr != error) return false;

    const uint_64 plain = data.size();
    reset_compress();
    if (!compress_block(data)) return false;

    const uint_64 stored = data.size();
    const uint_64 offset = _ostm->total();
    if (!key_.empty()) {
        const uint_64 iv[2]{stored, offset};
        aes_ctr<256>(key_.data(), (const uint_8*) iv).encrypt(data);
    }

    return _ostm->append(data) &&
           _ostm->append_ordinary(plain) &&
           _ostm->append_ordinary(stored);
}

decoder_v1::decoder_v1(istream* _istm, const std::string& _pswd)
    : decoder_base(_istm), key_(make_key(_pswd)) {
}

bool decoder_v1::is_accept(const istream& _istm) {
    bytes_view data = _istm.get(ARC_COMMON_HEAD_SIZE, sizeof(uint_32));
    if (data.empty()) return false;
    const uint_32 cipher = data.to<uint_32>();
    return ARC_AESC == cipher || ARC_NONE == cipher;
}

bool decoder_v1::is_encrypt() const { return encrypt_; }

uint_8 decoder_v1::read_meta(varmap& _archive, varmap& _tree) {
    (void) _archive;
    const istream* istm = get_stream();
    const bytes_view cipher_data = istm->get(ARC_COMMON_HEAD_SIZE, sizeof(uint_32));
    if (cipher_data.size() != sizeof(uint_32)) return decoder::badversion;
    encrypt_ = ARC_NONE != cipher_data.to<uint_32>();
    if (!encrypt_) key_.clear();

    bytes_view tail = istm->get(istm->total() - 20, 20);
    if (tail.size() != 20) return decoder::readfail;

    const uint_64 stored_size = tail.to<uint_64>(sizeof(uint_64));

    const uint_64 offset = istm->total() - 20 - stored_size;
    bytes data = istm->get(offset, stored_size).to_bytes();
    if (data.size() != stored_size) return decoder::readfail;

    if (!key_.empty()) {
        const uint_64 iv[2]{stored_size, offset};
        aes_ctr<256>(key_.data(), (const uint_8*) iv).decrypt(data);
    }
    const uint_64 plain_size = tail.to<uint_64>();
    reset_compress();
    if (!decompress_block(data, plain_size)) return decoder::dcmpsfail;
    return varsolid::to_varmap(data, _tree) ? decoder::success : decoder::badfsys;
}

uint_8 decoder_v1::read_block(const varvec& _blocks, uint_64& _pos, bytes& _plain) const {
    // a record and its check string must both fit before the trailing total
    if (_pos + 1 >= _blocks.size()) return decoder::badblock;

    const std::vector<uint_64> block_mesgs = _blocks[_pos++].to<std::vector<uint_64>>();
    const std::string block_check = _blocks[_pos++].to_string();
    if (block_mesgs.size() < 3 || block_check.empty()) return decoder::badblock;

    _plain = get_stream()->get(block_mesgs[2], block_mesgs[1]).to_bytes();
    if (_plain.size() != block_mesgs[1]) return decoder::readfail;

    if (!key_.empty()) {
        const uint_64 iv[2]{block_mesgs[1], block_mesgs[2]};
        aes_ctr<256>(key_.data(), (const uint_8*) iv).decrypt(_plain);
    }
    if (!decompress_block(_plain, block_mesgs[0])) return decoder::dcmpsfail;
    if (verify::exec(CHECK_TYPE, _plain) != block_check) return decoder::badblock;
    return decoder::success;
}
