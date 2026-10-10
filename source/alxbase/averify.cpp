/*****************************************************************/ /**
 * \file   averify.cpp
 * \brief  Data validation and verification
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "averify.h"
#include "afactory.h"

#include <iomanip>
#include <sstream>

#ifdef _WIN32
#else
#    include <cpuid.h>
#    include <immintrin.h>
#endif

using namespace alx;

// gcc and msvc have _rotl/_rotr built in; clang does not, hence these two templates
#ifdef __clang__
template <typename T>
inline T _rotl(T _v, uint_8 _s) {
    constexpr uint_8 bitmask = (sizeof(T) << 3) - 1;
    _s &= bitmask;
    return 0 == _s ? _v : (_v << _s) | (_v >> ((bitmask + 1) - _s));
}
template <typename T>
T _rotr(T _v, uint_8 _s) {
    constexpr uint_8 bitmask = (sizeof(T) << 3) - 1;
    _s &= bitmask;
    return 0 == _s ? _v : (_v >> _s) | (_v << ((bitmask + 1) - _s));
}
#endif

typedef factory<verify> verify_factory;
template <typename T>
using verify_product = product<T, verify>;

namespace {
    // SSE4.2 CRC32 is CPUID leaf 1, ECX bit 20; false whenever TRY_CRC_HARD is off
    bool is_crc32_supported() {
#if TRY_CRC_HARD
#    ifdef _WIN32
        int cpu_info[4]{0};
        __cpuid(cpu_info, 1);
        return (cpu_info[2] & (1 << 20)) != 0;
#    else
        unsigned int cpu_info[4]{0};
        __get_cpuid(1, &cpu_info[0], &cpu_info[1], &cpu_info[2], &cpu_info[3]);
        return (cpu_info[2] & (1 << 20)) != 0;
#    endif
#else
        return false;
#endif
    }
    static const bool is_crc32_supported_v = is_crc32_supported();
    // SHA-NI is CPUID leaf 7, sub-leaf 0, EBX bit 29; false whenever TRY_SHA_HARD is off
    bool is_sha_ni_supported() {
#if TRY_SHA_HARD
#    ifdef _WIN32
        int cpu_info[4]{0};
        __cpuid(cpu_info, 0);
        if (cpu_info[0] < 7) return false;
        __cpuidex(cpu_info, 7, 0);
        return (cpu_info[1] & (1 << 29)) != 0;
#    else
        unsigned int cpu_info[4]{0};
        __get_cpuid(0, &cpu_info[0], &cpu_info[1], &cpu_info[2], &cpu_info[3]);
        if (cpu_info[0] < 7) return false;
        __cpuid_count(7, 0, cpu_info[0], cpu_info[1], cpu_info[2], cpu_info[3]);
        return (cpu_info[1] & (1 << 29)) != 0;
#    endif
#else
        return false;
#endif
    }
    static const bool is_sha_ni_supported_v = is_sha_ni_supported();
}

class sha_1
    : public verify_product<sha_1> {
public:
    sha_1() { clear(); }
    virtual void update(const uint_8* _data, uint_64 _len) override {
        is_sha_ni_supported_v ? update_hard(_data, _len) : update_soft(_data, _len);
    }
    virtual std::string hexdigest() override {
        finalize();
        std::stringstream ss;
        for (auto val : m_state) ss << std::hex << std::setfill('0') << std::setw(8) << val;
        return ss.str();
    }
    virtual bytes bytedigest() override {
        finalize();
        return bytes(m_state, sizeof(m_state));
    }
    virtual bool hardcal() override { return is_sha_ni_supported_v; }
    virtual void clear() override {
        memcpy(m_state, s_state, sizeof(s_state));
        m_count = 0;
        m_cache_index = 0;
    }

private:
    void finalize() {
        // 72 = the longest pad run (64 bytes, when exactly 56 are cached) plus the 8-byte length
        uint_8 padding[72] = {0};
        padding[0] = 0x80;
        uint_64 pad_len = (m_cache_index < 56) ? (56 - m_cache_index) : (120 - m_cache_index);
        for (int i = 0; i < 8; ++i) padding[pad_len + i] = (m_count >> ((7 - i) * 8)) & 0xFF;
        update(padding, pad_len + 8);
    }

private:
    void update_soft(const uint_8* _data, uint_64 _len) {
        m_count += _len * 8;
        if (m_cache_index + _len < 64) {
            memcpy(m_cache + m_cache_index, _data, _len);
            m_cache_index += _len;
            return;
        }
        uint_32 buffer[80];
        if (m_cache_index != 0) {
            uint_64 size = 64 - m_cache_index;
            memcpy(m_cache + m_cache_index, _data, size);
            _len -= size;
            _data += size;
            m_cache_index = 0;
            transform_soft(m_cache, buffer);
        }
        while (_len >= 64) {
            // transform_soft reads the block as uint_32 words, so the bytes go through the aligned cache
            memcpy(m_cache, _data, 64);
            transform_soft(m_cache, buffer);
            _len -= 64;
            _data += 64;
        }
        if (_len != 0) {
            memcpy(m_cache, _data, _len);
            m_cache_index = _len;
        }
    }
    void transform_soft(const uint_8 _block[64], uint_32 _buffer[64]) {
        for (int t = 0; t < 16; ++t) _buffer[t] = byte_reverse(((const uint_32*) _block)[t]);

        for (int t = 16; t < 80; ++t)
            _buffer[t] = _rotl(_buffer[t - 3] ^ _buffer[t - 8] ^ _buffer[t - 14] ^ _buffer[t - 16], 1);

        uint_32 a = m_state[0];
        uint_32 b = m_state[1];
        uint_32 c = m_state[2];
        uint_32 d = m_state[3];
        uint_32 e = m_state[4];

        for (int t = 0; t < 80; ++t) {
            uint_32 f, k;
            if (t <= 19) {
                f = (b & c) | ((~b) & d);
                k = s_key[0];
            } else if (t <= 39) {
                f = b ^ c ^ d;
                k = s_key[1];
            } else if (t <= 59) {
                f = (b & c) | (b & d) | (c & d);
                k = s_key[2];
            } else {
                f = b ^ c ^ d;
                k = s_key[3];
            }

            uint_32 temp = (_rotl(a, 5) + f + e + k + _buffer[t]);
            e = d;
            d = c;
            c = _rotl(b, 30);
            b = a;
            a = temp;
        }

        m_state[0] += a;
        m_state[1] += b;
        m_state[2] += c;
        m_state[3] += d;
        m_state[4] += e;
    }
    void update_hard(const uint_8* _data, uint_64 _len) {
#if TRY_SHA_HARD
        m_count += _len * 8;
        if (m_cache_index + _len < 64) {
            memcpy(m_cache + m_cache_index, _data, _len);
            m_cache_index += _len;
            return;
        }
        // SHA-NI takes A in the top lane, so the state loads reversed and E rides in the top lane of e0
        __m128i msg[4];
        __m128i abcd, e0;
        abcd = _mm_loadu_si128((const __m128i*) m_state);
        e0 = _mm_set_epi32(m_state[4], 0, 0, 0);
        abcd = _mm_shuffle_epi32(abcd, 0x1B);
        if (m_cache_index != 0) {
            uint_64 size = 64 - m_cache_index;
            memcpy(m_cache + m_cache_index, _data, size);
            _len -= size;
            _data += size;
            m_cache_index = 0;
            transfrom_hard(abcd, e0, msg, m_cache);
        }
        while (_len >= 64) {
            transfrom_hard(abcd, e0, msg, _data);
            _len -= 64;
            _data += 64;
        }
        if (_len != 0) {
            memcpy(m_cache, _data, _len);
            m_cache_index = _len;
        }
        // back to A..D; the loop holds the state in abcd/e0, so m_state is stale until here
        abcd = _mm_shuffle_epi32(abcd, 0x1B);
        _mm_storeu_si128((__m128i*) m_state, abcd);
        m_state[4] = _mm_extract_epi32(e0, 3);
#endif
    }
    void transfrom_hard(__m128i& abcd, __m128i& e0, __m128i msg[4], const uint_8* _block) {
#if TRY_SHA_HARD
        __m128i abcd_save = abcd, e0_save = e0, e1;

        msg[0] = _mm_loadu_si128((const __m128i*) (_block + 0));
        msg[0] = _mm_shuffle_epi8(msg[0], s_mask);
        e0 = _mm_add_epi32(e0, msg[0]);
        e1 = abcd;
        abcd = _mm_sha1rnds4_epu32(abcd, e0, 0);

        msg[1] = _mm_loadu_si128((const __m128i*) (_block + 16));
        msg[1] = _mm_shuffle_epi8(msg[1], s_mask);
        e1 = _mm_sha1nexte_epu32(e1, msg[1]);
        e0 = abcd;
        abcd = _mm_sha1rnds4_epu32(abcd, e1, 0);
        msg[0] = _mm_sha1msg1_epu32(msg[0], msg[1]);

        msg[2] = _mm_loadu_si128((const __m128i*) (_block + 32));
        msg[2] = _mm_shuffle_epi8(msg[2], s_mask);
        e0 = _mm_sha1nexte_epu32(e0, msg[2]);
        e1 = abcd;
        abcd = _mm_sha1rnds4_epu32(abcd, e0, 0);
        msg[1] = _mm_sha1msg1_epu32(msg[1], msg[2]);
        msg[0] = _mm_xor_si128(msg[0], msg[2]);

        msg[3] = _mm_loadu_si128((const __m128i*) (_block + 48));
        msg[3] = _mm_shuffle_epi8(msg[3], s_mask);
        e1 = _mm_sha1nexte_epu32(e1, msg[3]);
        e0 = abcd;
        msg[0] = _mm_sha1msg2_epu32(msg[0], msg[3]);
        abcd = _mm_sha1rnds4_epu32(abcd, e1, 0);
        msg[2] = _mm_sha1msg1_epu32(msg[2], msg[3]);
        msg[1] = _mm_xor_si128(msg[1], msg[3]);

        e0 = _mm_sha1nexte_epu32(e0, msg[0]);
        e1 = abcd;
        msg[1] = _mm_sha1msg2_epu32(msg[1], msg[0]);
        abcd = _mm_sha1rnds4_epu32(abcd, e0, 0);
        msg[3] = _mm_sha1msg1_epu32(msg[3], msg[0]);
        msg[2] = _mm_xor_si128(msg[2], msg[0]);

        e1 = _mm_sha1nexte_epu32(e1, msg[1]);
        e0 = abcd;
        msg[2] = _mm_sha1msg2_epu32(msg[2], msg[1]);
        abcd = _mm_sha1rnds4_epu32(abcd, e1, 1);
        msg[0] = _mm_sha1msg1_epu32(msg[0], msg[1]);
        msg[3] = _mm_xor_si128(msg[3], msg[1]);

        e0 = _mm_sha1nexte_epu32(e0, msg[2]);
        e1 = abcd;
        msg[3] = _mm_sha1msg2_epu32(msg[3], msg[2]);
        abcd = _mm_sha1rnds4_epu32(abcd, e0, 1);
        msg[1] = _mm_sha1msg1_epu32(msg[1], msg[2]);
        msg[0] = _mm_xor_si128(msg[0], msg[2]);

        e1 = _mm_sha1nexte_epu32(e1, msg[3]);
        e0 = abcd;
        msg[0] = _mm_sha1msg2_epu32(msg[0], msg[3]);
        abcd = _mm_sha1rnds4_epu32(abcd, e1, 1);
        msg[2] = _mm_sha1msg1_epu32(msg[2], msg[3]);
        msg[1] = _mm_xor_si128(msg[1], msg[3]);

        e0 = _mm_sha1nexte_epu32(e0, msg[0]);
        e1 = abcd;
        msg[1] = _mm_sha1msg2_epu32(msg[1], msg[0]);
        abcd = _mm_sha1rnds4_epu32(abcd, e0, 1);
        msg[3] = _mm_sha1msg1_epu32(msg[3], msg[0]);
        msg[2] = _mm_xor_si128(msg[2], msg[0]);

        e1 = _mm_sha1nexte_epu32(e1, msg[1]);
        e0 = abcd;
        msg[2] = _mm_sha1msg2_epu32(msg[2], msg[1]);
        abcd = _mm_sha1rnds4_epu32(abcd, e1, 1);
        msg[0] = _mm_sha1msg1_epu32(msg[0], msg[1]);
        msg[3] = _mm_xor_si128(msg[3], msg[1]);

        e0 = _mm_sha1nexte_epu32(e0, msg[2]);
        e1 = abcd;
        msg[3] = _mm_sha1msg2_epu32(msg[3], msg[2]);
        abcd = _mm_sha1rnds4_epu32(abcd, e0, 2);
        msg[1] = _mm_sha1msg1_epu32(msg[1], msg[2]);
        msg[0] = _mm_xor_si128(msg[0], msg[2]);

        e1 = _mm_sha1nexte_epu32(e1, msg[3]);
        e0 = abcd;
        msg[0] = _mm_sha1msg2_epu32(msg[0], msg[3]);
        abcd = _mm_sha1rnds4_epu32(abcd, e1, 2);
        msg[2] = _mm_sha1msg1_epu32(msg[2], msg[3]);
        msg[1] = _mm_xor_si128(msg[1], msg[3]);

        e0 = _mm_sha1nexte_epu32(e0, msg[0]);
        e1 = abcd;
        msg[1] = _mm_sha1msg2_epu32(msg[1], msg[0]);
        abcd = _mm_sha1rnds4_epu32(abcd, e0, 2);
        msg[3] = _mm_sha1msg1_epu32(msg[3], msg[0]);
        msg[2] = _mm_xor_si128(msg[2], msg[0]);

        e1 = _mm_sha1nexte_epu32(e1, msg[1]);
        e0 = abcd;
        msg[2] = _mm_sha1msg2_epu32(msg[2], msg[1]);
        abcd = _mm_sha1rnds4_epu32(abcd, e1, 2);
        msg[0] = _mm_sha1msg1_epu32(msg[0], msg[1]);
        msg[3] = _mm_xor_si128(msg[3], msg[1]);

        e0 = _mm_sha1nexte_epu32(e0, msg[2]);
        e1 = abcd;
        msg[3] = _mm_sha1msg2_epu32(msg[3], msg[2]);
        abcd = _mm_sha1rnds4_epu32(abcd, e0, 2);
        msg[1] = _mm_sha1msg1_epu32(msg[1], msg[2]);
        msg[0] = _mm_xor_si128(msg[0], msg[2]);

        e1 = _mm_sha1nexte_epu32(e1, msg[3]);
        e0 = abcd;
        msg[0] = _mm_sha1msg2_epu32(msg[0], msg[3]);
        abcd = _mm_sha1rnds4_epu32(abcd, e1, 3);
        msg[2] = _mm_sha1msg1_epu32(msg[2], msg[3]);
        msg[1] = _mm_xor_si128(msg[1], msg[3]);

        e0 = _mm_sha1nexte_epu32(e0, msg[0]);
        e1 = abcd;
        msg[1] = _mm_sha1msg2_epu32(msg[1], msg[0]);
        abcd = _mm_sha1rnds4_epu32(abcd, e0, 3);
        msg[3] = _mm_sha1msg1_epu32(msg[3], msg[0]);
        msg[2] = _mm_xor_si128(msg[2], msg[0]);

        e1 = _mm_sha1nexte_epu32(e1, msg[1]);
        e0 = abcd;
        msg[2] = _mm_sha1msg2_epu32(msg[2], msg[1]);
        abcd = _mm_sha1rnds4_epu32(abcd, e1, 3);
        msg[3] = _mm_xor_si128(msg[3], msg[1]);

        e0 = _mm_sha1nexte_epu32(e0, msg[2]);
        e1 = abcd;
        msg[3] = _mm_sha1msg2_epu32(msg[3], msg[2]);
        abcd = _mm_sha1rnds4_epu32(abcd, e0, 3);

        e1 = _mm_sha1nexte_epu32(e1, msg[3]);
        e0 = abcd;
        abcd = _mm_sha1rnds4_epu32(abcd, e1, 3);

        e0 = _mm_sha1nexte_epu32(e0, e0_save);
        abcd = _mm_add_epi32(abcd, abcd_save);
#endif
    }

private:
    // m_count counts bits, m_cache_index counts the bytes of m_cache held
    uint_32 m_state[5];
    uint_64 m_count{0};
    uint_8 m_cache[64];
    uint_64 m_cache_index{0};
#if TRY_SHA_HARD
    static const __m128i s_mask;
#endif
    static const uint_32 s_key[4];
    static const uint_32 s_state[5];
};

class sha_256
    : public verify_product<sha_256> {
public:
    sha_256() { clear(); }
    virtual void update(const uint_8* _data, uint_64 _len) override {
        is_sha_ni_supported_v ? update_hard(_data, _len) : update_soft(_data, _len);
    }
    virtual std::string hexdigest() override {
        finalize();
        std::stringstream ss;
        for (auto val : m_state) ss << std::hex << std::setfill('0') << std::setw(8) << val;
        return ss.str();
    }
    virtual bytes bytedigest() override {
        finalize();
        return bytes(m_state, sizeof(m_state));
    }
    virtual bool hardcal() override { return is_sha_ni_supported_v; }
    virtual void clear() override {
        memcpy(m_state, s_state, sizeof(s_state));
        m_count = 0;
        m_cache_index = 0;
    }

private:
    void finalize() {
        // 72 = the longest pad run (64 bytes, when exactly 56 are cached) plus the 8-byte length
        uint_8 padding[72] = {0};
        padding[0] = 0x80;
        uint_64 pad_len = (m_cache_index < 56) ? (56 - m_cache_index) : (120 - m_cache_index);
        for (int i = 0; i < 8; ++i) padding[pad_len + i] = (m_count >> ((7 - i) * 8)) & 0xFF;
        update(padding, pad_len + 8);
    }

private:
    void update_soft(const uint_8* _data, uint_64 _len) {
        m_count += _len * 8;
        if (m_cache_index + _len < 64) {
            memcpy(m_cache + m_cache_index, _data, _len);
            m_cache_index += _len;
            return;
        }
        uint_32 buffer[80];
        if (m_cache_index != 0) {
            uint_64 size = 64 - m_cache_index;
            memcpy(m_cache + m_cache_index, _data, size);
            _len -= size;
            _data += size;
            m_cache_index = 0;
            transform_soft(m_cache, buffer);
        }
        while (_len >= 64) {
            // transform_soft reads the block as uint_32 words, so the bytes go through the aligned cache
            memcpy(m_cache, _data, 64);
            transform_soft(m_cache, buffer);
            _len -= 64;
            _data += 64;
        }
        if (_len != 0) {
            memcpy(m_cache, _data, _len);
            m_cache_index = _len;
        }
    }
    void transform_soft(const uint_8 _block[64], uint_32 _buffer[64]) {
        for (int t = 0; t < 16; ++t) _buffer[t] = byte_reverse(((const uint_32*) _block)[t]);

        for (int t = 16; t < 64; ++t) {
            uint_32 s0 = _rotr(_buffer[t - 15], 7) ^ _rotr(_buffer[t - 15], 18) ^ (_buffer[t - 15] >> 3);
            uint_32 s1 = _rotr(_buffer[t - 2], 17) ^ _rotr(_buffer[t - 2], 19) ^ (_buffer[t - 2] >> 10);
            _buffer[t] = _buffer[t - 16] + s0 + _buffer[t - 7] + s1;
        }

        uint_32 a = m_state[0];
        uint_32 b = m_state[1];
        uint_32 c = m_state[2];
        uint_32 d = m_state[3];
        uint_32 e = m_state[4];
        uint_32 f = m_state[5];
        uint_32 g = m_state[6];
        uint_32 h = m_state[7];

        for (int t = 0; t < 64; ++t) {
            uint_32 S1 = _rotr(e, 6) ^ _rotr(e, 11) ^ _rotr(e, 25);
            uint_32 ch = (e & f) ^ ((~e) & g);
            uint_32 temp1 = h + S1 + ch + s_key[t] + _buffer[t];

            uint_32 S0 = _rotr(a, 2) ^ _rotr(a, 13) ^ _rotr(a, 22);
            uint_32 maj = (a & b) ^ (a & c) ^ (b & c);
            uint_32 temp2 = S0 + maj;

            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }

        m_state[0] += a;
        m_state[1] += b;
        m_state[2] += c;
        m_state[3] += d;
        m_state[4] += e;
        m_state[5] += f;
        m_state[6] += g;
        m_state[7] += h;
    }
    void update_hard(const uint_8* _data, uint_64 _len) {
#if TRY_SHA_HARD
        m_count += _len * 8;
        if (m_cache_index + _len < 64) {
            memcpy(m_cache + m_cache_index, _data, _len);
            m_cache_index += _len;
            return;
        }

        // pack A..H out of m_state into the ABEF / CDGH lane order transform_hard works in
        __m128i state[2], cache[6];
        cache[5] = _mm_loadu_si128(((const __m128i*) m_state) + 0);
        state[1] = _mm_loadu_si128(((const __m128i*) m_state) + 1);
        cache[5] = _mm_shuffle_epi32(cache[5], 0xB1);
        state[1] = _mm_shuffle_epi32(state[1], 0x1B);
        state[0] = _mm_alignr_epi8(cache[5], state[1], 8);
        state[1] = _mm_blend_epi16(state[1], cache[5], 0xF0);

        if (m_cache_index != 0) {
            uint_64 size = 64 - m_cache_index;
            memcpy(m_cache + m_cache_index, _data, size);
            _len -= size;
            _data += size;
            m_cache_index = 0;
            transform_hard(state, cache, m_cache);
        }
        while (_len >= 64) {
            transform_hard(state, cache, _data);
            _len -= 64;
            _data += 64;
        }
        if (_len != 0) {
            memcpy(m_cache, _data, _len);
            m_cache_index = _len;
        }

        // the inverse packing, back to A..H; the loop left m_state alone, so it is written only here
        cache[5] = _mm_shuffle_epi32(state[0], 0x1B);
        state[1] = _mm_shuffle_epi32(state[1], 0xB1);
        state[0] = _mm_blend_epi16(cache[5], state[1], 0xF0);
        state[1] = _mm_alignr_epi8(state[1], cache[5], 8);

        _mm_storeu_si128((__m128i*) &m_state[0], state[0]);
        _mm_storeu_si128((__m128i*) &m_state[4], state[1]);
#endif
    }
    void transform_hard(__m128i _state[2], __m128i _cache[6], const uint_8 _block[64]) {
#if TRY_SHA_HARD
        __m128i abef = _state[0], cdgh = _state[1];

        _cache[4] = _mm_loadu_si128((const __m128i*) (_block + 0));
        _cache[0] = _mm_shuffle_epi8(_cache[4], s_mask);
        _cache[4] = _mm_add_epi32(_cache[0], s_key128[0]);
        _state[1] = _mm_sha256rnds2_epu32(_state[1], _state[0], _cache[4]);
        _cache[4] = _mm_shuffle_epi32(_cache[4], 0x0E);
        _state[0] = _mm_sha256rnds2_epu32(_state[0], _state[1], _cache[4]);

        _cache[1] = _mm_loadu_si128((const __m128i*) (_block + 16));
        _cache[1] = _mm_shuffle_epi8(_cache[1], s_mask);
        _cache[4] = _mm_add_epi32(_cache[1], s_key128[1]);
        _state[1] = _mm_sha256rnds2_epu32(_state[1], _state[0], _cache[4]);
        _cache[4] = _mm_shuffle_epi32(_cache[4], 0x0E);
        _state[0] = _mm_sha256rnds2_epu32(_state[0], _state[1], _cache[4]);
        _cache[0] = _mm_sha256msg1_epu32(_cache[0], _cache[1]);

        _cache[2] = _mm_loadu_si128((const __m128i*) (_block + 32));
        _cache[2] = _mm_shuffle_epi8(_cache[2], s_mask);
        _cache[4] = _mm_add_epi32(_cache[2], s_key128[2]);
        _state[1] = _mm_sha256rnds2_epu32(_state[1], _state[0], _cache[4]);
        _cache[4] = _mm_shuffle_epi32(_cache[4], 0x0E);
        _state[0] = _mm_sha256rnds2_epu32(_state[0], _state[1], _cache[4]);
        _cache[1] = _mm_sha256msg1_epu32(_cache[1], _cache[2]);

        _cache[3] = _mm_loadu_si128((const __m128i*) (_block + 48));
        _cache[3] = _mm_shuffle_epi8(_cache[3], s_mask);
        _cache[4] = _mm_add_epi32(_cache[3], s_key128[3]);
        _state[1] = _mm_sha256rnds2_epu32(_state[1], _state[0], _cache[4]);
        _cache[5] = _mm_alignr_epi8(_cache[3], _cache[2], 4);
        _cache[0] = _mm_add_epi32(_cache[0], _cache[5]);
        _cache[0] = _mm_sha256msg2_epu32(_cache[0], _cache[3]);
        _cache[4] = _mm_shuffle_epi32(_cache[4], 0x0E);
        _state[0] = _mm_sha256rnds2_epu32(_state[0], _state[1], _cache[4]);
        _cache[2] = _mm_sha256msg1_epu32(_cache[2], _cache[3]);

        _cache[4] = _mm_add_epi32(_cache[0], s_key128[4]);
        _state[1] = _mm_sha256rnds2_epu32(_state[1], _state[0], _cache[4]);
        _cache[5] = _mm_alignr_epi8(_cache[0], _cache[3], 4);
        _cache[1] = _mm_add_epi32(_cache[1], _cache[5]);
        _cache[1] = _mm_sha256msg2_epu32(_cache[1], _cache[0]);
        _cache[4] = _mm_shuffle_epi32(_cache[4], 0x0E);
        _state[0] = _mm_sha256rnds2_epu32(_state[0], _state[1], _cache[4]);
        _cache[3] = _mm_sha256msg1_epu32(_cache[3], _cache[0]);

        _cache[4] = _mm_add_epi32(_cache[1], s_key128[5]);
        _state[1] = _mm_sha256rnds2_epu32(_state[1], _state[0], _cache[4]);
        _cache[5] = _mm_alignr_epi8(_cache[1], _cache[0], 4);
        _cache[2] = _mm_add_epi32(_cache[2], _cache[5]);
        _cache[2] = _mm_sha256msg2_epu32(_cache[2], _cache[1]);
        _cache[4] = _mm_shuffle_epi32(_cache[4], 0x0E);
        _state[0] = _mm_sha256rnds2_epu32(_state[0], _state[1], _cache[4]);
        _cache[0] = _mm_sha256msg1_epu32(_cache[0], _cache[1]);

        _cache[4] = _mm_add_epi32(_cache[2], s_key128[6]);
        _state[1] = _mm_sha256rnds2_epu32(_state[1], _state[0], _cache[4]);
        _cache[5] = _mm_alignr_epi8(_cache[2], _cache[1], 4);
        _cache[3] = _mm_add_epi32(_cache[3], _cache[5]);
        _cache[3] = _mm_sha256msg2_epu32(_cache[3], _cache[2]);
        _cache[4] = _mm_shuffle_epi32(_cache[4], 0x0E);
        _state[0] = _mm_sha256rnds2_epu32(_state[0], _state[1], _cache[4]);
        _cache[1] = _mm_sha256msg1_epu32(_cache[1], _cache[2]);

        _cache[4] = _mm_add_epi32(_cache[3], s_key128[7]);
        _state[1] = _mm_sha256rnds2_epu32(_state[1], _state[0], _cache[4]);
        _cache[5] = _mm_alignr_epi8(_cache[3], _cache[2], 4);
        _cache[0] = _mm_add_epi32(_cache[0], _cache[5]);
        _cache[0] = _mm_sha256msg2_epu32(_cache[0], _cache[3]);
        _cache[4] = _mm_shuffle_epi32(_cache[4], 0x0E);
        _state[0] = _mm_sha256rnds2_epu32(_state[0], _state[1], _cache[4]);
        _cache[2] = _mm_sha256msg1_epu32(_cache[2], _cache[3]);

        _cache[4] = _mm_add_epi32(_cache[0], s_key128[8]);
        _state[1] = _mm_sha256rnds2_epu32(_state[1], _state[0], _cache[4]);
        _cache[5] = _mm_alignr_epi8(_cache[0], _cache[3], 4);
        _cache[1] = _mm_add_epi32(_cache[1], _cache[5]);
        _cache[1] = _mm_sha256msg2_epu32(_cache[1], _cache[0]);
        _cache[4] = _mm_shuffle_epi32(_cache[4], 0x0E);
        _state[0] = _mm_sha256rnds2_epu32(_state[0], _state[1], _cache[4]);
        _cache[3] = _mm_sha256msg1_epu32(_cache[3], _cache[0]);

        _cache[4] = _mm_add_epi32(_cache[1], s_key128[9]);
        _state[1] = _mm_sha256rnds2_epu32(_state[1], _state[0], _cache[4]);
        _cache[5] = _mm_alignr_epi8(_cache[1], _cache[0], 4);
        _cache[2] = _mm_add_epi32(_cache[2], _cache[5]);
        _cache[2] = _mm_sha256msg2_epu32(_cache[2], _cache[1]);
        _cache[4] = _mm_shuffle_epi32(_cache[4], 0x0E);
        _state[0] = _mm_sha256rnds2_epu32(_state[0], _state[1], _cache[4]);
        _cache[0] = _mm_sha256msg1_epu32(_cache[0], _cache[1]);

        _cache[4] = _mm_add_epi32(_cache[2], s_key128[10]);
        _state[1] = _mm_sha256rnds2_epu32(_state[1], _state[0], _cache[4]);
        _cache[5] = _mm_alignr_epi8(_cache[2], _cache[1], 4);
        _cache[3] = _mm_add_epi32(_cache[3], _cache[5]);
        _cache[3] = _mm_sha256msg2_epu32(_cache[3], _cache[2]);
        _cache[4] = _mm_shuffle_epi32(_cache[4], 0x0E);
        _state[0] = _mm_sha256rnds2_epu32(_state[0], _state[1], _cache[4]);
        _cache[1] = _mm_sha256msg1_epu32(_cache[1], _cache[2]);

        _cache[4] = _mm_add_epi32(_cache[3], s_key128[11]);
        _state[1] = _mm_sha256rnds2_epu32(_state[1], _state[0], _cache[4]);
        _cache[5] = _mm_alignr_epi8(_cache[3], _cache[2], 4);
        _cache[0] = _mm_add_epi32(_cache[0], _cache[5]);
        _cache[0] = _mm_sha256msg2_epu32(_cache[0], _cache[3]);
        _cache[4] = _mm_shuffle_epi32(_cache[4], 0x0E);
        _state[0] = _mm_sha256rnds2_epu32(_state[0], _state[1], _cache[4]);
        _cache[2] = _mm_sha256msg1_epu32(_cache[2], _cache[3]);

        _cache[4] = _mm_add_epi32(_cache[0], s_key128[12]);
        _state[1] = _mm_sha256rnds2_epu32(_state[1], _state[0], _cache[4]);
        _cache[5] = _mm_alignr_epi8(_cache[0], _cache[3], 4);
        _cache[1] = _mm_add_epi32(_cache[1], _cache[5]);
        _cache[1] = _mm_sha256msg2_epu32(_cache[1], _cache[0]);
        _cache[4] = _mm_shuffle_epi32(_cache[4], 0x0E);
        _state[0] = _mm_sha256rnds2_epu32(_state[0], _state[1], _cache[4]);
        _cache[3] = _mm_sha256msg1_epu32(_cache[3], _cache[0]);

        _cache[4] = _mm_add_epi32(_cache[1], s_key128[13]);
        _state[1] = _mm_sha256rnds2_epu32(_state[1], _state[0], _cache[4]);
        _cache[5] = _mm_alignr_epi8(_cache[1], _cache[0], 4);
        _cache[2] = _mm_add_epi32(_cache[2], _cache[5]);
        _cache[2] = _mm_sha256msg2_epu32(_cache[2], _cache[1]);
        _cache[4] = _mm_shuffle_epi32(_cache[4], 0x0E);
        _state[0] = _mm_sha256rnds2_epu32(_state[0], _state[1], _cache[4]);

        _cache[4] = _mm_add_epi32(_cache[2], s_key128[14]);
        _state[1] = _mm_sha256rnds2_epu32(_state[1], _state[0], _cache[4]);
        _cache[5] = _mm_alignr_epi8(_cache[2], _cache[1], 4);
        _cache[3] = _mm_add_epi32(_cache[3], _cache[5]);
        _cache[3] = _mm_sha256msg2_epu32(_cache[3], _cache[2]);
        _cache[4] = _mm_shuffle_epi32(_cache[4], 0x0E);
        _state[0] = _mm_sha256rnds2_epu32(_state[0], _state[1], _cache[4]);

        _cache[4] = _mm_add_epi32(_cache[3], s_key128[15]);
        _state[1] = _mm_sha256rnds2_epu32(_state[1], _state[0], _cache[4]);
        _cache[4] = _mm_shuffle_epi32(_cache[4], 0x0E);
        _state[0] = _mm_sha256rnds2_epu32(_state[0], _state[1], _cache[4]);

        _state[0] = _mm_add_epi32(_state[0], abef);
        _state[1] = _mm_add_epi32(_state[1], cdgh);
#endif
    }

    // m_count counts bits, m_cache_index counts the bytes of m_cache held
    uint_32 m_state[8];
    uint_64 m_count{0};
    uint_8 m_cache[64];
    uint_64 m_cache_index{0};
#if TRY_SHA_HARD
    static const __m128i s_mask;
    static const __m128i* s_key128;
#endif
    static const uint_32 s_key[64];
    static const uint_32 s_state[8];
};

template <typename CRC, CRC POLY>
class crc_state {
protected:
    // one table per (CRC, POLY) pair: variants differing only in init, xorout or reflection share it
    crc_state() {
        static const bool initialized = [] { init(); return true; }();
        (void) initialized;
    }
    static void init() {
        CRC crc;
        for (uint_64 i = 0; i < 0x100U; i++) {
            crc = CRC(i << ((sizeof(CRC) - 1) << 3));
            for (int bit = 0; bit < 8; ++bit) {
                // bit_align_v<1, n-1> is the register's top bit, where the polynomial's implicit x^n sits
                if (crc & alx::bit_align_v<1, sizeof(CRC) * 8 - 1>)
                    crc = ((crc << 1) ^ POLY);
                else crc <<= 1;
            }
            s_table[i] = crc;
        }
    }

protected:
    static CRC s_table[0x100U];
};
template <typename CRC, CRC POLY>
CRC crc_state<CRC, POLY>::s_table[0x100U]{0};

template <typename CRC, CRC POLY, CRC INIT, CRC XORO, bool INRE, bool OURE>
class crc_base : protected crc_state<CRC, POLY> {
protected:
    using crc_state<CRC, POLY>::s_table;
    crc_base() {}
    void update(const uint_8* _data, uint_64 _len) {
        while (_len-- > 0) m_crc = (m_crc << 8) ^ s_table[(m_crc >> ((sizeof(CRC) - 1) << 3)) ^ (s_inre ? bit_reverse(*(_data++)) : *(_data++))];
    }
    std::string hexdigest() {
        return hexdigest((s_oure ? bit_reverse(m_crc) : m_crc) ^ s_xoro);
    }
    bytes bytedigest() {
        return bytes::from_ordinary((s_oure ? bit_reverse(m_crc) : m_crc) ^ s_xoro);
    }
    static std::string hexdigest(CRC _crc) {
        std::stringstream ss;
        ss << std::hex << std::setw(sizeof(CRC) << 1) << std::setfill('0') << (uint_64) _crc;
        return ss.str();
    }

protected:
    CRC m_crc{s_init};
    static constexpr CRC s_init{INIT};
    static constexpr CRC s_xoro{XORO};
    static constexpr bool s_inre{INRE};
    static constexpr bool s_oure{OURE};
};

#define CRC_IMPL(NAME, TYPE, POLY, INIT, XORO, INRE, OURE)                                                        \
    class NAME : public verify_product<NAME>, public crc_base<TYPE, POLY, INIT, XORO, INRE, OURE> {               \
    public:                                                                                                       \
        NAME() {}                                                                                                 \
        virtual void update(const uint_8* _data, uint_64 _len) override { return crc_base::update(_data, _len); } \
        virtual std::string hexdigest() override { return crc_base::hexdigest(); }                                \
        virtual bytes bytedigest() override { return crc_base::bytedigest(); }                                    \
        virtual bool hardcal() override { return false; }                                                         \
        virtual void clear() override { m_crc = s_init; }                                                         \
    };

CRC_IMPL(crc_8, uint_8, 0x07, 0x00, 0x00, false, false);
CRC_IMPL(crc_8_itu, uint_8, 0x07, 0x00, 0x55, false, false);
CRC_IMPL(crc_8_rohc, uint_8, 0x07, 0xFF, 0x00, true, true);
CRC_IMPL(crc_8_maxim, uint_8, 0x31, 0x00, 0x00, true, true);

CRC_IMPL(crc_16_ibm, uint_16, 0x8005, 0x0000, 0x0000, true, true);
CRC_IMPL(crc_16_maxim, uint_16, 0x8005, 0x0000, 0xFFFF, true, true);
CRC_IMPL(crc_16_usb, uint_16, 0x8005, 0xFFFF, 0xFFFF, true, true);
CRC_IMPL(crc_16_modbus, uint_16, 0x8005, 0xFFFF, 0x0000, true, true);
CRC_IMPL(crc_16_ccitt, uint_16, 0x1021, 0x0000, 0x0000, true, true);
CRC_IMPL(crc_16_ccitt_false, uint_16, 0x1021, 0xFFFF, 0x0000, false, false);
CRC_IMPL(crc_16_x25, uint_16, 0x1021, 0xFFFF, 0xFFFF, true, true);
CRC_IMPL(crc_16_xmode, uint_16, 0x1021, 0x0000, 0x0000, false, false);
CRC_IMPL(crc_16_xmode2, uint_16, 0x8408, 0x0000, 0x0000, true, true);
CRC_IMPL(crc_16_dnp, uint_16, 0x3D65, 0x0000, 0xFFFF, true, true);

CRC_IMPL(crc_32, uint_32, 0x04C11DB7, 0xFFFFFFFF, 0xFFFFFFFF, true, true);
#if TRY_CRC_HARD
class crc_32_c
    : public verify_product<crc_32_c>,
      public crc_base<uint_32, 0x1EDC6F41, 0xFFFFFFFF, 0xFFFFFFFF, true, true> {
public:
    crc_32_c() {}
    virtual void update(const uint_8* _data, uint_64 _len) override {
        if (!is_crc32_supported_v) return crc_base::update(_data, _len);
        const uint_8* end = _data + _len;
        while (_data + 8 <= end) {
            m_crc = (uint_32) _mm_crc32_u64(m_crc, m_interpret<uint_64>(_data));
            _data += 8;
        }
        while (_data < end) m_crc = _mm_crc32_u8(m_crc, *_data++);
    }
    // the instruction returns the already-reflected value, so both digests skip the out-reflection
    virtual std::string hexdigest() override {
        return is_crc32_supported_v ? crc_base::hexdigest(m_crc ^ s_xoro) : crc_base::hexdigest();
    }
    virtual bytes bytedigest() override {
        return is_crc32_supported_v ? bytes::from_ordinary(m_crc ^ s_xoro) : crc_base::bytedigest();
    }
    virtual bool hardcal() override { return is_crc32_supported_v; }
    virtual void clear() override { m_crc = s_init; }
};
#else
// without TRY_CRC_HARD crc_32_c is the plain table variant, like every other CRC
CRC_IMPL(crc_32_c, uint_32, 0x1EDC6F41, 0xFFFFFFFF, 0xFFFFFFFF, true, true);
#endif
CRC_IMPL(crc_32_koopman, uint_32, 0x741B8CD7, 0xFFFFFFFF, 0xFFFFFFFF, true, true);
CRC_IMPL(crc_32_mpeg_2, uint_32, 0x04C11DB7, 0xFFFFFFFF, 0x00000000, false, false);

CRC_IMPL(crc_64_iso, uint_64, 0x000000000000001B, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, true, true);
CRC_IMPL(crc_64_ecma, uint_64, 0x42F0E1EBA9EA3693, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, true, true);

std::vector<std::string> alx::verify::list() {
    std::vector<std::string> result;
    for (auto it : verify_factory::type_map()) result.push_back(it.first);
    return result;
}

verify* alx::verify::create(const std::string& _type) { return verify_factory::create(_type); }

verify* alx::verify::create(const COMMON_TYPE _type) {
    switch (_type) {
    case CRC_32: return new crc_32();
    case CRC_32C: return new crc_32_c();
    case SHA_1: return new sha_1();
    case SHA_256: return new sha_256();
    default: return nullptr;
    }
}

std::string alx::verify::exec(const COMMON_TYPE _type, const uint_8* _data, uint_64 _len) {
    std::string result;
    verify* v = create(_type);
    if (nullptr == v) return result;
    v->update(_data, _len);
    result = v->hexdigest();
    delete v;
    return result;
}

std::string alx::verify::exec(const std::string& _type, const uint_8* _data, uint_64 _len) {
    std::string result;
    verify* v = create(_type);
    if (nullptr == v) return result;
    v->update(_data, _len);
    result = v->hexdigest();
    delete v;
    return result;
}

const uint_32 sha_1::s_state[5]{0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
const uint_32 sha_1::s_key[4]{0x5A827999, 0x6ED9EBA1, 0x8F1BBCDC, 0xCA62C1D6};
const uint_32 sha_256::s_state[8]{0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A, 0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19};
const uint_32 sha_256::s_key[64]{
    0x428A2F98, 0x71374491, 0xB5C0FBCF, 0xE9B5DBA5,
    0x3956C25B, 0x59F111F1, 0x923F82A4, 0xAB1C5ED5,
    0xD807AA98, 0x12835B01, 0x243185BE, 0x550C7DC3,
    0x72BE5D74, 0x80DEB1FE, 0x9BDC06A7, 0xC19BF174,
    0xE49B69C1, 0xEFBE4786, 0x0FC19DC6, 0x240CA1CC,
    0x2DE92C6F, 0x4A7484AA, 0x5CB0A9DC, 0x76F988DA,
    0x983E5152, 0xA831C66D, 0xB00327C8, 0xBF597FC7,
    0xC6E00BF3, 0xD5A79147, 0x06CA6351, 0x14292967,
    0x27B70A85, 0x2E1B2138, 0x4D2C6DFC, 0x53380D13,
    0x650A7354, 0x766A0ABB, 0x81C2C92E, 0x92722C85,
    0xA2BFE8A1, 0xA81A664B, 0xC24B8B70, 0xC76C51A3,
    0xD192E819, 0xD6990624, 0xF40E3585, 0x106AA070,
    0x19A4C116, 0x1E376C08, 0x2748774C, 0x34B0BCB5,
    0x391C0CB3, 0x4ED8AA4A, 0x5B9CCA4F, 0x682E6FF3,
    0x748F82EE, 0x78A5636F, 0x84C87814, 0x8CC70208,
    0x90BEFFFA, 0xA4506CEB, 0xBEF9A3F7, 0xC67178F2};

#if TRY_SHA_HARD
// byte-swap masks: the block words come in little-endian, the SHA words are big-endian
const __m128i sha_1::s_mask = _mm_set_epi64x(0x0001020304050607ULL, 0x08090A0B0C0D0E0FULL);
const __m128i sha_256::s_mask = _mm_set_epi64x(0x0C0D0E0F08090A0BULL, 0x0405060700010203ULL);
const __m128i* sha_256::s_key128 = (const __m128i*) s_key;
#endif