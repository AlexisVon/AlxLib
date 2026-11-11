/*****************************************************************/ /**
 * \file   afpacker_v2.cpp
 * \brief  The v2 archive envelope — AES-256-GCM, key derivation head, sealed index
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "afpacker_v2.h"

#include "aaes.h"
#include "aalgo.h"
#include "aplatform.h"
#include "autility.h"
#include "avarsolid.h"

using namespace alx;
using namespace alx::fpacker;
using namespace alx::fpacker::detail;

namespace {

    void write_le32(uint_8* _dst, uint_32 _value) {
        for (int i = 0; i < 4; ++i) _dst[i] = (uint_8) (_value >> (i * 8));
    }

    void write_le64(uint_8* _dst, uint_64 _value) {
        for (int i = 0; i < 8; ++i) _dst[i] = (uint_8) (_value >> (i * 8));
    }

    uint_32 read_le32(const uint_8* _src) {
        return (uint_32) _src[0] | ((uint_32) _src[1] << 8) | ((uint_32) _src[2] << 16) | ((uint_32) _src[3] << 24);
    }

    bytes make_aad(const bytes& _head, uint_64 _plain, uint_64 _stored, uint_64 _offset) {
        bytes aad = _head;
        uint_8 sizes[3 * sizeof(uint_64)];
        write_le64(sizes, _plain);
        write_le64(sizes + sizeof(uint_64), _stored);
        write_le64(sizes + 2 * sizeof(uint_64), _offset);
        aad.append(sizes, sizeof(sizes));
        return aad;
    }

    void make_nonce(uint_32 _kind, uint_64 _offset, uint_8 _nonce[12]) {
        write_le32(_nonce, _kind);
        write_le64(_nonce + sizeof(uint_32), _offset);
    }

    /// Seal _data in place and append the tag; a fresh object makes start() and finish() infallible
    void seal(const bytes& _key, uint_32 _kind, uint_64 _offset, const bytes& _aad, bytes& _data) {
        uint_8 nonce[12];
        make_nonce(_kind, _offset, nonce);
        aes_gcm<256> gcm(_key.data(), nonce);
        gcm.start(_aad);
        gcm.encrypt(_data.data(), _data.size());
        bytes tag;
        gcm.finish(tag);
        _data.append(tag);
    }

    uint_8 unseal(const bytes& _key, uint_32 _kind, uint_64 _offset, const bytes& _aad,
                  bytes& _data, const uint_8 _tag[ARC_AESG_TAG_SIZE]) {
        uint_8 nonce[12];
        make_nonce(_kind, _offset, nonce);
        aes_gcm<256> gcm(_key.data(), nonce);
        gcm.start(_aad);
        gcm.decrypt(_data.data(), _data.size());
        bytes computed;
        if (!gcm.finish(computed)) return decoder::badauth;

        uint_8 diff = 0;
        for (uint_64 i = 0; i < ARC_AESG_TAG_SIZE; ++i) diff |= computed[i] ^ _tag[i];
        return 0 == diff ? decoder::success : decoder::badauth;
    }
}

encoder_v2::encoder_v2(bool _cmps, uint_64 _blck_size, const std::string& _pswd, uint_32 _iterations)
    : encoder_base(_cmps, _blck_size), pswd_(_pswd), iterations_(_iterations) {
}

bool encoder_v2::write_head(ostream* _ostm) {
    uint_8 salt[ARC_AESG_SALT_SIZE];
    bool fellback = false;
    for (uint_64 i = 0; i < 2; ++i) {
        uint_64 value = sys_rand();
        if (0 == value) {
            if (!is_igerr_randfallback()) {
                report_code(std::string(), encoder::randfail);
                return false;
            }
            value = alx::rand();
            if (0 == value) {
                report_code(std::string(), encoder::randfail);
                return false;
            }
            fellback = true;
        }
        write_le64(salt + i * sizeof(uint_64), value);
    }
    if (fellback) report_code(std::string(), encoder::randfallback);

    key_ = bytes(ARC_AESG_KEY_SIZE);
    pbkdf2_hmac_sha256((const uint_8*) pswd_.data(), pswd_.size(), salt, ARC_AESG_SALT_SIZE,
                       iterations_, key_.data(), (uint_32) ARC_AESG_KEY_SIZE);

    bytes version(ARC_AESG_HEAD_SIZE);
    m_interpret(version.data(), ARC_AESG);
    version[sizeof(uint_32)] = ARC_AESG_KDF_PBKDF2;
    write_le32(version.data() + ARC_AESG_ITER_OFF, iterations_);
    memcpy(version.data() + ARC_AESG_SALT_OFF, salt, ARC_AESG_SALT_SIZE);

    head_ = bytes(ARC_COMMON_HEAD_SIZE);
    m_interpret(head_.data(), ARC_HEAD);
    m_interpret(head_.data() + sizeof(uint_32), (uint_32) (is_compress() ? ARC_CMPS : ARC_NONE));
    head_.append(version);

    return _ostm->append(version);
}

uint_8 encoder_v2::write_block(const bytes& _plain, uint_64 _offset, bytes& _stored, varvec& _blocks) {
    _stored = _plain;
    if (!compress_block(_stored)) return encoder::cmpsfail;

    const uint_64 plain_size = _plain.size();
    const uint_64 stored_size = _stored.size() + ARC_AESG_TAG_SIZE;
    seal(key_, ARC_AESG_KIND_BLOCK, _offset, make_aad(head_, plain_size, stored_size, _offset), _stored);

    std::vector<uint_64> record;
    record.push_back(plain_size);
    record.push_back(stored_size);
    record.push_back(_offset);
    _blocks.push_back(record);
    return encoder::success;
}

bool encoder_v2::write_meta(ostream* _ostm, const varmap& _archive, const varmap& _fsys) {
    varmap root;
    root.insert("fsys", _fsys);
    root.insert("info", _archive);

    variant* error{nullptr};
    bytes data = varsolid::to_bytes(root, &error);
    if (nullptr != error) return false;

    const uint_64 plain_size = data.size();
    reset_compress();
    if (!compress_block(data)) return false;

    const uint_64 stored_size = data.size() + ARC_AESG_TAG_SIZE;
    const uint_64 offset = _ostm->total();
    seal(key_, ARC_AESG_KIND_INDEX, offset, make_aad(head_, plain_size, stored_size, offset), data);

    return _ostm->append(data) && _ostm->append_ordinary(plain_size) && _ostm->append_ordinary(stored_size);
}

decoder_v2::decoder_v2(istream* _istm, const std::string& _pswd)
    : decoder_base(_istm), pswd_(_pswd) {
}

bool decoder_v2::is_accept(const istream& _istm) {
    bytes_view data = _istm.get(ARC_COMMON_HEAD_SIZE, sizeof(uint_32));
    if (data.empty()) return false;
    return ARC_AESG == data.to<uint_32>();
}

bool decoder_v2::is_encrypt() const { return true; }

uint_8 decoder_v2::read_meta(varmap& _archive, varmap& _fsys) {
    const istream* istm = get_stream();
    bytes_view head = istm->get(0, ARC_AESG_HEAD_LEN);
    if (head.size() != ARC_AESG_HEAD_LEN) return decoder::badversion;
    if (ARC_AESG != head.to<uint_32>(ARC_COMMON_HEAD_SIZE)) return decoder::badversion;
    if (ARC_AESG_KDF_PBKDF2 != head[ARC_COMMON_HEAD_SIZE + sizeof(uint_32)]) return decoder::badparams;

    const uint_32 iterations = read_le32(head.data() + ARC_COMMON_HEAD_SIZE + ARC_AESG_ITER_OFF);
    if (0 == iterations || iterations > ARC_AESG_MAX_ITER) return decoder::badparams;

    head_ = head.to_bytes();
    key_ = bytes(ARC_AESG_KEY_SIZE);
    pbkdf2_hmac_sha256((const uint_8*) pswd_.data(), pswd_.size(),
                       head.data() + ARC_COMMON_HEAD_SIZE + ARC_AESG_SALT_OFF, ARC_AESG_SALT_SIZE,
                       iterations, key_.data(), (uint_32) ARC_AESG_KEY_SIZE);

    const uint_64 total = istm->total();
    if (total < ARC_AESG_HEAD_LEN + ARC_AESG_TAIL_SIZE) return decoder::badversion;

    bytes_view tail = istm->get(total - ARC_AESG_TAIL_SIZE, ARC_AESG_TAIL_SIZE);
    if (tail.size() != ARC_AESG_TAIL_SIZE) return decoder::readfail;
    const uint_64 plain_size = tail.to<uint_64>();
    const uint_64 stored_size = tail.to<uint_64>(sizeof(uint_64));
    if (stored_size < ARC_AESG_TAG_SIZE || stored_size > total - ARC_AESG_HEAD_LEN - ARC_AESG_TAIL_SIZE) return decoder::badversion;

    const uint_64 offset = total - ARC_AESG_TAIL_SIZE - stored_size;
    bytes data = istm->get(offset, stored_size).to_bytes();
    if (data.size() != stored_size) return decoder::readfail;

    uint_8 tag[ARC_AESG_TAG_SIZE];
    memcpy(tag, data.data() + stored_size - ARC_AESG_TAG_SIZE, ARC_AESG_TAG_SIZE);
    data.resize(stored_size - ARC_AESG_TAG_SIZE);

    const uint_8 code = unseal(key_, ARC_AESG_KIND_INDEX, offset, make_aad(head_, plain_size, stored_size, offset), data, tag);
    if (decoder::success != code) return code;

    reset_compress();
    if (!decompress_block(data, plain_size)) return decoder::dcmpsfail;

    varmap root;
    if (!varsolid::to_varmap(data, root)) return decoder::badfsys;
    if (!root.contain("fsys") || !root.value("fsys").is<varmap>()) return decoder::badfsys;

    _fsys = root.value("fsys").to<varmap>();
    _archive.clear();
    if (root.contain("info") && root.value("info").is<varmap>()) _archive = root.value("info").to<varmap>();
    return decoder::success;
}

uint_8 decoder_v2::read_block(const varvec& _blocks, uint_64& _pos, bytes& _plain) const {
    const std::vector<uint_64> record = _blocks[_pos++].to<std::vector<uint_64>>();
    if (record.size() < 3) return decoder::badblock;

    const uint_64 plain_size = record[0];
    const uint_64 stored_size = record[1];
    const uint_64 offset = record[2];
    if (stored_size < ARC_AESG_TAG_SIZE) return decoder::badblock;

    bytes data = get_stream()->get(offset, stored_size).to_bytes();
    if (data.size() != stored_size) return decoder::readfail;

    uint_8 tag[ARC_AESG_TAG_SIZE];
    memcpy(tag, data.data() + stored_size - ARC_AESG_TAG_SIZE, ARC_AESG_TAG_SIZE);
    data.resize(stored_size - ARC_AESG_TAG_SIZE);

    const uint_8 code = unseal(key_, ARC_AESG_KIND_BLOCK, offset, make_aad(head_, plain_size, stored_size, offset), data, tag);
    if (decoder::success != code) return code;

    _plain = data;
    if (!decompress_block(_plain, plain_size)) return decoder::dcmpsfail;
    return decoder::success;
}
