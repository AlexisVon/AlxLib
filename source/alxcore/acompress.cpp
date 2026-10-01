/*****************************************************************/ /**
 * \file   acompress.cpp
 * \brief  Data compression (integrated LZ4 algorithm)
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "acompress.h"
#include <lz4.h>
#include <zlib.h>
#include <zstd.h>

using namespace alx;
using namespace alx::compress;

constexpr uint_64 MAX_DICT_SIZE = 64 * 1024;

bytes encoder_lz4::sexec(const bytes_view& _src, uint_16 _speed) {
    return sexec(_src.data(), (int_32) _src.size(), _speed);
}

bytes encoder_lz4::sexec(const uint_8* _src, int_32 _size, uint_16 _speed) {
    if (nullptr == _src || _size <= 0) return bytes();

    const int max_cmps_size = LZ4_COMPRESSBOUND((int) _size);
    if (max_cmps_size <= 0) return bytes();
    bytes result(max_cmps_size);

    const int rst_cmps_size = LZ4_compress_fast(
        (const char*) _src,
        (char*) result.data(),
        (int) _size,
        (int) result.size(),
        _speed);

    if (rst_cmps_size <= 0) return bytes();
    else return result.resize(rst_cmps_size), result;
}

bool encoder_lz4::sexec(const bytes_view& _src, bytes& _out, uint_16 _speed) {
    return sexec(_src.data(), (int_32) _src.size(), _out, _speed);
}

bool encoder_lz4::sexec(const uint_8* _src, int_32 _size, bytes& _out, uint_16 _speed) {
    if (nullptr == _src || _size <= 0) return false;

    const int max_cmps_size = LZ4_COMPRESSBOUND((int) _size);
    if (max_cmps_size <= 0) return false;
    _out.resize(max_cmps_size);

    const int rst_cmps_size = LZ4_compress_fast(
        (const char*) _src,
        (char*) _out.data(),
        (int) _size,
        (int) _out.size(),
        _speed);

    if (rst_cmps_size <= 0) return false;
    else return _out.resize(rst_cmps_size), true;
}

encoder_lz4::encoder_lz4()
    : cmps_(LZ4_createStream()), buff_(new uint_8[MAX_DICT_SIZE]{0}) {
}

encoder_lz4::~encoder_lz4() {
    LZ4_freeStream((LZ4_stream_t*) cmps_);
    delete[] buff_;
}

bytes encoder_lz4::exec(const bytes_view& _src, uint_16 _speed) {
    return exec(_src.cdata(), (uint_32) _src.size(), _speed);
}

bytes encoder_lz4::exec(const uint_8* _src, int_32 _size, uint_16 _speed) {
    if (nullptr == _src || _size <= 0) return bytes();

    const int max_cmps_size = LZ4_compressBound((int) _size);
    if (max_cmps_size <= 0) return bytes();
    bytes result(max_cmps_size);

    const int rst_cmps_size = LZ4_compress_fast_continue(
        (LZ4_stream_t*) cmps_,
        (const char*) _src,
        (char*) result.data(),
        (int) _size,
        (int) result.size(),
        _speed);

    if (rst_cmps_size <= 0) return bytes();
    else return result.resize(rst_cmps_size), update(), result;
}

void encoder_lz4::clear() {
    LZ4_resetStream_fast((LZ4_stream_t*) cmps_);
}

void encoder_lz4::reset() {
    LZ4_resetStream((LZ4_stream_t*) cmps_);
}

void encoder_lz4::update() {
    LZ4_saveDict((LZ4_stream_t*) cmps_, (char*) buff_, MAX_DICT_SIZE);
}

bytes decoder_lz4::sexec(const bytes_view& _src, int_32 _source_size) {
    return sexec(_src.data(), (int_32) _src.size(), _source_size);
}

bytes decoder_lz4::sexec(const uint_8* _src, int_32 _size, int_32 _source_size) {
    if (nullptr == _src || _size <= 0 || _source_size <= 0) return bytes();
    if (_size > _source_size && _size > LZ4_COMPRESSBOUND((int) _source_size)) return bytes();

    // _source_size is the caller's claim, not a fact: bound it by LZ4's worst-case expansion
    const int_64 max_origin = (int_64) _size * 255 + 16;
    if ((int_64) _source_size > max_origin) _source_size = (int_32) (max_origin > max_int_32 ? (int_64) max_int_32 : max_origin);

    bytes result(_source_size);
    int rst_sour_size = LZ4_decompress_safe(
        (const char*) _src,
        (char*) result.data(),
        (int) _size,
        (int) result.size());

    if (rst_sour_size <= 0) return bytes();
    else return result.resize(rst_sour_size), result;
}

bool decoder_lz4::sexec(const bytes_view& _src, bytes& _out) {
    return sexec(_src.data(), (int_32) _src.size(), _out);
}

bool decoder_lz4::sexec(const uint_8* _src, int_32 _size, bytes& _out) {
    if (nullptr == _src || _size <= 0) return false;
    if (_size > (int_32) _out.size() && _size > LZ4_COMPRESSBOUND((int) _out.size())) return false;

    int rst_sour_size = LZ4_decompress_safe(
        (const char*) _src,
        (char*) _out.data(),
        (int) _size,
        (int) _out.size());

    if (rst_sour_size <= 0) return false;
    else return _out.resize(rst_sour_size), true;
}

decoder_lz4::decoder_lz4()
    : cmps_(LZ4_createStreamDecode()), buff_(new uint_8[MAX_DICT_SIZE]{0}) {
    reset();
}

decoder_lz4::~decoder_lz4() {
    LZ4_freeStreamDecode((LZ4_streamDecode_t*) cmps_);
    delete[] buff_;
}

bytes decoder_lz4::exec(const bytes_view& _src, int_32 _source_size) {
    return exec(_src.data(), (int_32) _src.size(), _source_size);
}

bytes decoder_lz4::exec(const uint_8* _src, int_32 _size, int_32 _source_size) {
    if (nullptr == _src || _size <= 0) return bytes();
    if (_size > _source_size && _size > LZ4_COMPRESSBOUND((int) _source_size)) return bytes();

    const int_64 max_origin = (int_64) _size * 255 + 16;
    if ((int_64) _source_size > max_origin) {
        _source_size = (int_32) (max_origin > max_int_32 ? (int_64) max_int_32 : max_origin);
    }

    bytes result(static_cast<size_t>(_source_size));

    const int rst_sour_size = LZ4_decompress_safe_continue(
        (LZ4_streamDecode_t*) cmps_,
        (const char*) _src,
        (char*) result.data(),
        (int) _size,
        (int) result.size());

    if (rst_sour_size <= 0) return bytes();
    else return result.resize(rst_sour_size), update(result), result;
}

void decoder_lz4::reset() {
    LZ4_setStreamDecode((LZ4_streamDecode_t*) cmps_, nullptr, 0);
}

void decoder_lz4::update(const bytes_view& _data) {
    uint_64 size = alx::min_value(MAX_DICT_SIZE, _data.size());
    memcpy(buff_, _data.data() + _data.size() - size, size);
    LZ4_setStreamDecode((LZ4_streamDecode_t*) cmps_, (const char*) buff_, (int) size);
}

// +16 is zlib's gzip-wrapper flag: window bits 31, not a window size
static constexpr int GZIP_WINDOW_BITS = MAX_WBITS + 16;

bytes encoder_gzip::sexec(const bytes_view& _src, uint_16 _level) {
    return sexec(_src.data(), (int_32) _src.size(), _level);
}

bytes encoder_gzip::sexec(const uint_8* _src, int_32 _size, uint_16 _level) {
    if (nullptr == _src || _size <= 0) return bytes();

    z_stream strm{};
    if (deflateInit2(&strm, _level, Z_DEFLATED, GZIP_WINDOW_BITS,
                     MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY) != Z_OK)
        return bytes();

    uLong bound = deflateBound(&strm, (uLong) _size);
    bytes result(bound);

    strm.next_in = (Bytef*) _src;
    strm.avail_in = (uInt) _size;
    strm.next_out = (Bytef*) result.data();
    strm.avail_out = (uInt) result.size();

    int ret = deflate(&strm, Z_FINISH);
    deflateEnd(&strm);

    if (ret != Z_STREAM_END) return bytes();
    return result.resize((uint_64) strm.total_out), result;
}

bool encoder_gzip::sexec(const bytes_view& _src, bytes& _out, uint_16 _level) {
    return sexec(_src.data(), (int_32) _src.size(), _out, _level);
}

bool encoder_gzip::sexec(const uint_8* _src, int_32 _size, bytes& _out, uint_16 _level) {
    if (nullptr == _src || _size <= 0) return false;

    z_stream strm{};
    if (deflateInit2(&strm, _level, Z_DEFLATED, GZIP_WINDOW_BITS,
                     MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY) != Z_OK)
        return false;

    uLong bound = deflateBound(&strm, (uLong) _size);
    _out.resize(bound);

    strm.next_in = (Bytef*) _src;
    strm.avail_in = (uInt) _size;
    strm.next_out = (Bytef*) _out.data();
    strm.avail_out = (uInt) _out.size();

    int ret = deflate(&strm, Z_FINISH);
    deflateEnd(&strm);

    if (ret != Z_STREAM_END) return false;
    return _out.resize((uint_64) strm.total_out), true;
}

encoder_gzip::encoder_gzip()
    : cmps_(new z_stream{}) {
    clear();
}

encoder_gzip::~encoder_gzip() {
    z_stream* strm = (z_stream*) cmps_;
    deflateEnd(strm);
    delete strm;
}

bytes encoder_gzip::exec(const bytes_view& _src, uint_16 _level) {
    return exec(_src.data(), (int_32) _src.size(), _level);
}

bytes encoder_gzip::exec(const uint_8* _src, int_32 _size, uint_16 _level) {
    if (nullptr == _src || _size <= 0) return bytes();

    z_stream* strm = (z_stream*) cmps_;
    // the ctor and reset() both leave the stream null: this is where it first gets initialized
    if (strm->state == nullptr) {

        if (deflateInit2(strm, _level, Z_DEFLATED, GZIP_WINDOW_BITS,
                         MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY) != Z_OK)
            return bytes();
    }

    uLong bound = deflateBound(strm, (uLong) _size);
    bytes result(bound);

    strm->next_in = (Bytef*) _src;
    strm->avail_in = (uInt) _size;
    strm->next_out = (Bytef*) result.data();
    strm->avail_out = (uInt) result.size();

    int ret = deflate(strm, Z_SYNC_FLUSH);
    if (ret != Z_OK && ret != Z_STREAM_END) return bytes();

    return result.resize((uint_8*) strm->next_out - result.data()), result;
}

void encoder_gzip::clear() {
    z_stream* strm = (z_stream*) cmps_;
    if (strm->state != nullptr) deflateReset(strm);
}

void encoder_gzip::reset() {
    z_stream* strm = (z_stream*) cmps_;
    if (strm->state != nullptr) {
        deflateEnd(strm);
        memset(strm, 0, sizeof(z_stream));
    }
}

bytes decoder_gzip::sexec(const bytes_view& _src) {
    return sexec(_src.data(), (int_32) _src.size());
}

bytes decoder_gzip::sexec(const uint_8* _src, int_32 _size) {
    if (nullptr == _src || _size <= 0) return bytes();

    z_stream strm{};
    if (inflateInit2(&strm, GZIP_WINDOW_BITS) != Z_OK) return bytes();

    strm.next_in = (Bytef*) _src;
    strm.avail_in = (uInt) _size;

    // 0X40000000 is 1 GiB, the ceiling on how far a small input may expand
    constexpr uint_64 max_out_bytes = 0X40000000ULL;
    bytes result(alx::min_value<uint_64>((uint_64) _size * 4, max_out_bytes));
    strm.next_out = (Bytef*) result.data();
    strm.avail_out = (uInt) result.size();

    int ret;
    do {
        ret = inflate(&strm, Z_NO_FLUSH);
        if (ret == Z_STREAM_END || ret == Z_OK) {
            if (strm.avail_out == 0 && ret != Z_STREAM_END) {

                uint_64 used = (uint_8*) strm.next_out - result.data();
                if (used >= max_out_bytes) break;
                result.resize(alx::min_value<uint_64>(used * 2, max_out_bytes));
                strm.next_out = (Bytef*) result.data() + used;
                strm.avail_out = (uInt) (result.size() - used);
            }
        } else break;
    } while (ret != Z_STREAM_END);

    inflateEnd(&strm);

    if (ret != Z_STREAM_END) return bytes();
    return result.resize((uint_64) strm.total_out), result;
}

bool decoder_gzip::sexec(const bytes_view& _src, bytes& _out) {
    return sexec(_src.data(), (int_32) _src.size(), _out);
}

bool decoder_gzip::sexec(const uint_8* _src, int_32 _size, bytes& _out) {
    if (nullptr == _src || _size <= 0) return false;

    z_stream strm{};
    if (inflateInit2(&strm, GZIP_WINDOW_BITS) != Z_OK) return false;

    strm.next_in = (Bytef*) _src;
    strm.avail_in = (uInt) _size;

    constexpr uint_64 max_out_bytes = 0X40000000ULL;
    if (_out.empty()) _out.resize(alx::min_value<uint_64>((uint_64) _size * 4, max_out_bytes));
    strm.next_out = (Bytef*) _out.data();
    strm.avail_out = (uInt) _out.size();

    int ret;
    do {
        ret = inflate(&strm, Z_NO_FLUSH);
        if (ret == Z_STREAM_END || ret == Z_OK) {
            if (strm.avail_out == 0 && ret != Z_STREAM_END) {
                uint_64 used = (uint_8*) strm.next_out - _out.data();
                if (_out.size() >= max_out_bytes) break;
                const uint_64 grow = alx::min_value<uint_64>(_out.size() * 2, max_out_bytes);
                _out.resize(grow);
                strm.next_out = (Bytef*) _out.data() + used;
                strm.avail_out = (uInt) (_out.size() - used);
            }
        } else break;
    } while (ret != Z_STREAM_END);

    inflateEnd(&strm);

    if (ret != Z_STREAM_END) return false;
    return _out.resize((uint_64) strm.total_out), true;
}

decoder_gzip::decoder_gzip()
    : cmps_(new z_stream{}) {
    reset();
}

decoder_gzip::~decoder_gzip() {
    z_stream* strm = (z_stream*) cmps_;
    inflateEnd(strm);
    delete strm;
}

bytes decoder_gzip::exec(const bytes_view& _src) {
    return exec(_src.data(), (int_32) _src.size());
}

bytes decoder_gzip::exec(const uint_8* _src, int_32 _size) {
    if (nullptr == _src || _size <= 0) return bytes();

    z_stream* strm = (z_stream*) cmps_;
    if (strm->state == nullptr) {
        if (inflateInit2(strm, GZIP_WINDOW_BITS) != Z_OK) return bytes();
    }

    strm->next_in = (Bytef*) _src;
    strm->avail_in = (uInt) _size;

    constexpr uint_64 max_out_bytes = 0X40000000ULL;
    bytes result(alx::min_value<uint_64>((uint_64) _size * 4, max_out_bytes));
    strm->next_out = (Bytef*) result.data();
    strm->avail_out = (uInt) result.size();

    int ret;
    do {
        ret = inflate(strm, Z_NO_FLUSH);
        // Z_BUF_ERROR: the chunk ends mid-member, so return what was decoded rather than fail
        if (ret == Z_BUF_ERROR) break;
        if (ret == Z_STREAM_END || ret == Z_OK) {
            if (strm->avail_out == 0) {
                uint_64 used = (uint_8*) strm->next_out - result.data();
                if (result.size() >= max_out_bytes) break;
                const uint_64 grow = alx::min_value<uint_64>(result.size() * 2, max_out_bytes);
                result.resize(grow);
                strm->next_out = (Bytef*) result.data() + used;
                strm->avail_out = (uInt) (result.size() - used);
            }
        } else {
            inflateEnd(strm);
            memset(strm, 0, sizeof(z_stream));
            return bytes();
        }
    } while (ret == Z_OK);

    if (ret != Z_STREAM_END && ret != Z_BUF_ERROR) return bytes();
    return result.resize((uint_8*) strm->next_out - result.data()), result;
}

void decoder_gzip::reset() {
    z_stream* strm = (z_stream*) cmps_;
    if (strm->state != nullptr) {
        inflateEnd(strm);
        memset(strm, 0, sizeof(z_stream));
    }
}

static bytes zstd_stream_decode(const uint_8* _src, int_32 _size, ZSTD_DCtx* _ctx, bool& _complete, bool& _failed) {
    _complete = false;
    _failed = false;

    constexpr uint_64 max_out_bytes = 0X40000000ULL;
    bytes result(alx::min_value<uint_64>((uint_64) _size * 4, max_out_bytes));
    if (result.null()) {
        _failed = true;
        return bytes();
    }

    ZSTD_inBuffer in{_src, (size_t) _size, 0};
    uint_64 used = 0;

    while (true) {
        ZSTD_outBuffer out{result.data() + used, (size_t) (result.size() - used), 0};

        const size_t ret = ZSTD_decompressStream(_ctx, &out, &in);
        if (ZSTD_isError(ret)) {
            _failed = true;
            return bytes();
        }
        used += (uint_64) out.pos;

        if (0 == ret) {
            _complete = true;
            break;
        }
        // a nonzero return with the output buffer not full: the input ran out mid-frame
        if (out.pos < out.size) break;

        if (result.size() >= max_out_bytes) {
            _failed = true;
            return bytes();
        }
        const uint_64 grow = alx::min_value<uint_64>(result.size() * 2, max_out_bytes);
        result.resize(grow);
        if (result.capacity() < grow) {
            _failed = true;
            return bytes();
        }
    }

    return result.resize(used), result;
}

bytes encoder_zstd::sexec(const bytes_view& _src, uint_16 _level) {
    return sexec(_src.data(), (int_32) _src.size(), _level);
}

bytes encoder_zstd::sexec(const uint_8* _src, int_32 _size, uint_16 _level) {
    if (nullptr == _src || _size <= 0) return bytes();

    const size_t bound = ZSTD_compressBound((size_t) _size);
    if (ZSTD_isError(bound)) return bytes();
    bytes result(bound);
    if (result.null()) return bytes();

    const size_t cmps_size = ZSTD_compress(result.data(), result.size(), _src, (size_t) _size, (int) _level);
    if (ZSTD_isError(cmps_size)) return bytes();
    return result.resize(cmps_size), result;
}

bool encoder_zstd::sexec(const bytes_view& _src, bytes& _out, uint_16 _level) {
    return sexec(_src.data(), (int_32) _src.size(), _out, _level);
}

bool encoder_zstd::sexec(const uint_8* _src, int_32 _size, bytes& _out, uint_16 _level) {
    if (nullptr == _src || _size <= 0) return false;

    const size_t bound = ZSTD_compressBound((size_t) _size);
    if (ZSTD_isError(bound)) return false;
    _out.resize(bound);

    const size_t cmps_size = ZSTD_compress(_out.data(), _out.size(), _src, (size_t) _size, (int) _level);
    if (ZSTD_isError(cmps_size)) return false;
    return _out.resize(cmps_size), true;
}

encoder_zstd::encoder_zstd()
    : cmps_(ZSTD_createCCtx()) {
}

encoder_zstd::~encoder_zstd() {
    if (cmps_ != nullptr) ZSTD_freeCCtx((ZSTD_CCtx*) cmps_);
}

bytes encoder_zstd::exec(const bytes_view& _src, uint_16 _level) {
    return exec(_src.data(), (int_32) _src.size(), _level);
}

bytes encoder_zstd::exec(const uint_8* _src, int_32 _size, uint_16 _level) {
    if (nullptr == _src || _size <= 0) return bytes();

    ZSTD_CCtx* ctx = (ZSTD_CCtx*) cmps_;
    if (ctx == nullptr) {
        ctx = ZSTD_createCCtx();
        if (ctx == nullptr) return bytes();
        cmps_ = ctx;
    }
    if (ZSTD_isError(ZSTD_CCtx_setParameter(ctx, ZSTD_c_compressionLevel, (int) _level))) return bytes();

    const uint_64 bound = (uint_64) ZSTD_compressBound((size_t) _size);
    // the flush below can outgrow the one-shot bound: twice it, plus slack for the overhead
    const uint_64 max_out_bytes = bound * 2 + 64;
    bytes result(bound);
    if (result.null()) return bytes();

    ZSTD_inBuffer in{_src, (size_t) _size, 0};
    uint_64 used = 0;

    size_t ret = 1;
    while (ret != 0) {
        ZSTD_outBuffer out{result.data() + used, (size_t) (result.size() - used), 0};

        ret = ZSTD_compressStream2(ctx, &out, &in, ZSTD_e_flush);
        if (ZSTD_isError(ret)) return bytes();
        used += (uint_64) out.pos;

        if (ret != 0) {
            if (result.size() >= max_out_bytes) return bytes();
            const uint_64 grow = alx::min_value<uint_64>(result.size() * 2, max_out_bytes);
            result.resize(grow);
            if (result.capacity() < grow) return bytes();
        }
    }

    return result.resize(used), result;
}

void encoder_zstd::clear() {
    ZSTD_CCtx* ctx = (ZSTD_CCtx*) cmps_;
    if (ctx != nullptr) ZSTD_CCtx_reset(ctx, ZSTD_reset_session_only);
}

void encoder_zstd::reset() {
    ZSTD_CCtx* ctx = (ZSTD_CCtx*) cmps_;
    if (ctx != nullptr) {
        ZSTD_freeCCtx(ctx);
        cmps_ = nullptr;
    }
}

bytes decoder_zstd::sexec(const bytes_view& _src) {
    return sexec(_src.data(), (int_32) _src.size());
}

bytes decoder_zstd::sexec(const uint_8* _src, int_32 _size) {
    if (nullptr == _src || _size <= 0) return bytes();

    constexpr uint_64 max_out_bytes = 0X40000000ULL;
    const unsigned long long content_size = ZSTD_getFrameContentSize(_src, (size_t) _size);
    if (ZSTD_CONTENTSIZE_ERROR == content_size) return bytes();
    if (ZSTD_CONTENTSIZE_UNKNOWN != content_size && content_size > max_out_bytes) return bytes();

    if (ZSTD_CONTENTSIZE_UNKNOWN != content_size) {
        bytes result((uint_64) content_size);
        if (result.null()) return bytes();

        const size_t rst_size = ZSTD_decompress(result.data(), result.size(), _src, (size_t) _size);
        if (ZSTD_isError(rst_size) || rst_size != (size_t) content_size) return bytes();
        return result;
    }

    ZSTD_DCtx* ctx = ZSTD_createDCtx();
    if (ctx == nullptr) return bytes();

    bool complete = false, failed = false;
    bytes result = zstd_stream_decode(_src, _size, ctx, complete, failed);
    ZSTD_freeDCtx(ctx);

    if (failed || !complete) return bytes();
    return result;
}

bool decoder_zstd::sexec(const bytes_view& _src, bytes& _out) {
    return sexec(_src.data(), (int_32) _src.size(), _out);
}

bool decoder_zstd::sexec(const uint_8* _src, int_32 _size, bytes& _out) {
    if (nullptr == _src || _size <= 0) return false;

    constexpr uint_64 max_out_bytes = 0X40000000ULL;
    const unsigned long long content_size = ZSTD_getFrameContentSize(_src, (size_t) _size);
    if (ZSTD_CONTENTSIZE_ERROR == content_size) return false;
    if (ZSTD_CONTENTSIZE_UNKNOWN != content_size && content_size > max_out_bytes) return false;

    if (ZSTD_CONTENTSIZE_UNKNOWN != content_size) {
        _out.resize((uint_64) content_size);
        const size_t rst_size = ZSTD_decompress(_out.data(), _out.size(), _src, (size_t) _size);
        if (ZSTD_isError(rst_size) || rst_size != (size_t) content_size) return false;
        return _out.resize((uint_64) content_size), true;
    }

    ZSTD_DCtx* ctx = ZSTD_createDCtx();
    if (ctx == nullptr) return false;

    bool complete = false, failed = false;
    bytes result = zstd_stream_decode(_src, _size, ctx, complete, failed);
    ZSTD_freeDCtx(ctx);

    if (failed || !complete) return false;
    return _out = result, true;
}

decoder_zstd::decoder_zstd()
    : cmps_(ZSTD_createDCtx()) {
}

decoder_zstd::~decoder_zstd() {
    if (cmps_ != nullptr) ZSTD_freeDCtx((ZSTD_DCtx*) cmps_);
}

bytes decoder_zstd::exec(const bytes_view& _src) {
    return exec(_src.data(), (int_32) _src.size());
}

bytes decoder_zstd::exec(const uint_8* _src, int_32 _size) {
    if (nullptr == _src || _size <= 0) return bytes();

    ZSTD_DCtx* ctx = (ZSTD_DCtx*) cmps_;
    if (ctx == nullptr) {
        ctx = ZSTD_createDCtx();
        if (ctx == nullptr) return bytes();
        cmps_ = ctx;
    }

    // an open frame is not a failure: the caller feeds the next chunk
    bool complete = false, failed = false;
    bytes result = zstd_stream_decode(_src, _size, ctx, complete, failed);
    if (failed) {
        reset();
        return bytes();
    }
    return result;
}

void decoder_zstd::reset() {
    ZSTD_DCtx* ctx = (ZSTD_DCtx*) cmps_;
    if (ctx != nullptr) {
        ZSTD_freeDCtx(ctx);
        cmps_ = nullptr;
    }
}
