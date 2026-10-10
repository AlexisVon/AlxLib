/*****************************************************************/ /**
 * \file   aaes.cpp
 * \brief  AES encryption and decryption algorithm
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "aaes.h"

#ifndef _WIN32
#    include <cpuid.h>
#    include <immintrin.h>
#endif

using namespace alx;

namespace {
    static const uint_8 sbox[256]{
        0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
        0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
        0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
        0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
        0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
        0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
        0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
        0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
        0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
        0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
        0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
        0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
        0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
        0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
        0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
        0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16};
    static const uint_8 rsbox[256]{
        0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
        0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
        0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
        0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
        0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
        0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
        0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
        0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
        0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
        0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
        0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
        0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
        0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
        0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
        0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
        0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d};
    static const uint_8 rcon[11]{
        0x8d, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36};
    bool is_aes_supported() {
#if TRY_AES_HARD
#    ifdef _WIN32
        int cpu_info[4]{0};
        __cpuid(cpu_info, 0);
        if (cpu_info[0] < 1) return false;
        __cpuid(cpu_info, 1);
        return (cpu_info[2] & (1 << 25)) != 0;
#    else
        unsigned int cpu_info[4]{0};
        __get_cpuid(0, &cpu_info[0], &cpu_info[1], &cpu_info[2], &cpu_info[3]);
        if (cpu_info[0] < 1) return false;
        __get_cpuid(1, &cpu_info[0], &cpu_info[1], &cpu_info[2], &cpu_info[3]);
        return (cpu_info[2] & (1 << 25)) != 0;
#    endif
#else
        return false;
#endif
    }
    bool is_pclmulqdq_supported() {
#if TRY_AES_HARD
#    ifdef _WIN32
        int cpu_info[4]{0};
        __cpuid(cpu_info, 0);
        if (cpu_info[0] < 1) return false;
        __cpuid(cpu_info, 1);
        return (cpu_info[2] & (1 << 1)) != 0;
#    else
        unsigned int cpu_info[4]{0};
        __get_cpuid(0, &cpu_info[0], &cpu_info[1], &cpu_info[2], &cpu_info[3]);
        if (cpu_info[0] < 1) return false;
        __get_cpuid(1, &cpu_info[0], &cpu_info[1], &cpu_info[2], &cpu_info[3]);
        return (cpu_info[2] & (1 << 1)) != 0;
#    endif
#else
        return false;
#endif
    }
    // The CPUID probes run once, at load: the dispatchers below just read the cached answer
    static const bool is_aes_supported_v = is_aes_supported();
    static const bool is_pclmulqdq_supported_v = is_pclmulqdq_supported();

#if TRY_AES_HARD
    // GCM numbers the bits of a block from the left, so its leftmost bit is the coefficient of
    // y^0; PCLMULQDQ multiplies the natural polynomial whose bit q is the coefficient of y^q.
    // Reversing the bits inside each byte -- byte order untouched -- is the map between the two,
    // and the field still reduces by the sparse y^128 + y^7 + y^2 + y + 1 GCM is named for
    inline __m128i ghash_reverse_bits(__m128i _v) {
        const __m128i nibbles = _mm_setr_epi8(0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe,
                                              0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf);
        const __m128i low_nibbles = _mm_set1_epi8(0x0F);
        return _mm_or_si128(
            _mm_slli_epi16(_mm_shuffle_epi8(nibbles, _mm_and_si128(_v, low_nibbles)), 4),
            _mm_shuffle_epi8(nibbles, _mm_and_si128(_mm_srli_epi16(_v, 4), low_nibbles)));
    }

    // The reduction constant is one byte wide, so the top half folds into the bottom in two
    // passes: the first leaves at most seven bits of carry, the second clears them
    inline __m128i ghash_reduce(__m128i _hi, __m128i _lo) {
        const __m128i reduction = _mm_set_epi64x(0, 0x87);
        __m128i low = _mm_clmulepi64_si128(_hi, reduction, 0x00);
        __m128i high = _mm_clmulepi64_si128(_hi, reduction, 0x01);
        _lo = _mm_xor_si128(_lo, _mm_xor_si128(low, _mm_slli_si128(high, 8)));
        return _mm_xor_si128(_lo, _mm_clmulepi64_si128(_mm_srli_si128(high, 8), reduction, 0x00));
    }

    // Both operands are reflected, so this is a plain carry-less multiplication over that field
    inline __m128i ghash_mul_reflected(__m128i _a, __m128i _b) {
        __m128i mid = _mm_xor_si128(_mm_clmulepi64_si128(_a, _b, 0x01),
                                    _mm_clmulepi64_si128(_a, _b, 0x10));
        return ghash_reduce(_mm_xor_si128(_mm_clmulepi64_si128(_a, _b, 0x11), _mm_srli_si128(mid, 8)),
                            _mm_xor_si128(_mm_clmulepi64_si128(_a, _b, 0x00), _mm_slli_si128(mid, 8)));
    }
#endif
}

// state[col][row] is byte 4 * col + row of the block: column-major, the order the round keys use too
typedef uint_8 state_type[aes_base::block_num][aes_base::block_num];

bool aes_base::hardcal() { return is_aes_supported_v; }

void aes_base::cipher(uint_8* _state, const uint_8* _round_key, const uint_8 _round_num) {
    is_aes_supported_v ? cipher_impl_hard(_state, _round_key, _round_num) : cipher_impl_soft(_state, _round_key, _round_num);
}

void aes_base::inv_cipher(uint_8* _state, const uint_8* _round_key, const uint_8 _round_num) {
    is_aes_supported_v ? inv_cipher_impl_hard(_state, _round_key, _round_num) : inv_cipher_impl_soft(_state, _round_key, _round_num);
}

// SSE2 is used here unconditionally, unlike the _hard entries: this one has no TRY_AES_HARD guard
void aes_base::xor_with_iv(uint_8* _state, const uint_8* _iv) {
    _mm_storeu_si128((__m128i*) _state,
                     _mm_xor_si128(
                         _mm_loadu_si128((__m128i*) _state),
                         _mm_loadu_si128((__m128i*) _iv)));
}

void aes_base::key_expansion(uint_8* _round_key, const uint_8* _key, const uint_8 _key_num, const uint_8 _round_num) {
    uint_8 i, j, k;
    uint_8 temp[4];
    memcpy(_round_key, _key, _key_num * 4);

    // i counts 32-bit words, not _round_key bytes: block_num of them per round
    for (i = _key_num; i < block_num * (_round_num + 1); ++i) {
        k = (i - 1) * 4;
        r_interpret<uint_32>(temp) = r_interpret<uint_32>(_round_key + k);

        if (i % _key_num == 0) {
            {
                const uint8_t u8tmp = temp[0];
                temp[0] = temp[1];
                temp[1] = temp[2];
                temp[2] = temp[3];
                temp[3] = u8tmp;
            }
            {
                temp[0] = sbox[temp[0]];
                temp[1] = sbox[temp[1]];
                temp[2] = sbox[temp[2]];
                temp[3] = sbox[temp[3]];
            }
            temp[0] = temp[0] ^ rcon[i / _key_num];
        }
        if (_key_num == 8 && i % _key_num == 4) {
            {
                temp[0] = sbox[temp[0]];
                temp[1] = sbox[temp[1]];
                temp[2] = sbox[temp[2]];
                temp[3] = sbox[temp[3]];
            }
        }
        j = i * 4;
        k = (i - _key_num) * 4;
        _round_key[j + 0] = _round_key[k + 0] ^ temp[0];
        _round_key[j + 1] = _round_key[k + 1] ^ temp[1];
        _round_key[j + 2] = _round_key[k + 2] ^ temp[2];
        _round_key[j + 3] = _round_key[k + 3] ^ temp[3];
    }
}

void aes_base::buf_padding(bytes& _buf, PADDING _mode) {
    uint_64 pad_len = 0X10LLU - (_buf.size() & 0X0FLLU);
    switch (_mode) {
    case aes_base::NONE: break;
    case aes_base::PKCS7:
        _buf.append(bytes(pad_len, (uint_8) pad_len));
        break;
    case aes_base::ZEROS:
        if (pad_len != 0X10LLU) _buf.append(bytes(pad_len, 0X00U));
        break;
    case aes_base::ANSIX923: {
        bytes pad(pad_len, 0X00U);
        pad[pad.size() - 1] = (uint_8) pad_len;
        _buf.append(pad);
        break;
    }
    case aes_base::ISO10126:
        _buf.resize(_buf.size() + pad_len);
        memset(_buf.data() + _buf.size() - pad_len, 0XFFU, pad_len - 1);
        _buf[_buf.size() - 1] = (uint_8) pad_len;
        break;
    default: break;
    }
}

void aes_base::buf_unpadding(bytes& _buf, PADDING _mode) {
    if (_buf.empty() || (_buf.size() & 0X0FLLU)) return;
    uint_8 pad_len = _buf[_buf.size() - 1];
    switch (_mode) {
    case aes_base::NONE: break;
    case aes_base::PKCS7:
        if (pad_len <= 0X10U && _buf.right(pad_len) == bytes(pad_len, pad_len))
            _buf.resize(_buf.size() - pad_len);
        break;
    case aes_base::ZEROS: {
        pad_len = 0;
        uint_64 index = _buf.size() - 1;
        while (pad_len < 0X0FU && _buf[index] == 0X00U) index--, pad_len++;
        _buf.resize(_buf.size() - pad_len);
        break;
    }
    case aes_base::ANSIX923:

        // pad_len > 0 is load-bearing: on a zero byte both pad_len - 1 counts below wrap
        if (pad_len > 0 && pad_len <= 0X10U &&
            _buf.mid(_buf.size() - pad_len, pad_len - 1) == bytes(pad_len - 1, 0X00U))
            _buf.resize(_buf.size() - pad_len);
        break;
    case aes_base::ISO10126:
        if (pad_len > 0 && pad_len <= 0X10U) _buf.resize(_buf.size() - pad_len);
        break;
    default: break;
    }
}

void aes_base::cipher_impl_soft(uint_8* _state, const uint_8* _round_key, const uint_8 _round_num) {
    add_round_key(_state, 0, _round_key);
    for (uint_8 round = 1;; ++round) {
        sub_bytes(_state);
        shift_rows(_state);
        if (_round_num == round) break;
        mix_columns(_state);
        add_round_key(_state, round, _round_key);
    }
    add_round_key(_state, _round_num, _round_key);
}

void aes_base::cipher_impl_hard(uint_8* _state, const uint_8* _round_key, const uint_8 _round_num) {
#if TRY_AES_HARD
    __m128i* round_key = (__m128i*) _round_key;
    __m128i data = _mm_loadu_si128((__m128i*) _state);
    data = _mm_xor_si128(data, round_key[0]);
    for (int i = 1; i < _round_num; i++) {
        data = _mm_aesenc_si128(data, round_key[i]);
    }
    data = _mm_aesenclast_si128(data, round_key[_round_num]);
    _mm_storeu_si128((__m128i*) _state, data);
#endif
}

void aes_base::inv_cipher_impl_soft(uint_8* _state, const uint_8* _round_key, const uint_8 _round_num) {
    add_round_key(_state, _round_num, _round_key);
    for (uint_8 round = _round_num - 1;; --round) {
        inv_shift_rows(_state);
        inv_sub_bytes(_state);
        add_round_key(_state, round, _round_key);
        if (0 == round) break;
        inv_mix_columns(_state);
    }
}

void aes_base::inv_cipher_impl_hard(uint_8* _state, const uint_8* _round_key, const uint_8 _round_num) {
#if TRY_AES_HARD
    __m128i* round_key = (__m128i*) _round_key;
    __m128i data = _mm_loadu_si128((__m128i*) _state);
    data = _mm_xor_si128(data, round_key[_round_num]);
    for (int i = _round_num - 1; i > 0; i--) {
        data = _mm_aesdec_si128(data, _mm_aesimc_si128(round_key[i]));
    }
    data = _mm_aesdeclast_si128(data, round_key[0]);
    _mm_storeu_si128((__m128i*) _state, data);
#endif
}

void aes_base::add_round_key(uint_8* _state, uint_8 _round, const uint_8* _key) {
    state_type& state = *(state_type*) _state;
    for (uint_8 i = 0; i < block_num; ++i)
        for (uint_8 j = 0; j < block_num; ++j) state[i][j] ^= _key[(_round * block_len) + (i * block_num) + j];
}

void aes_base::sub_bytes(uint_8* _state) {
    for (uint_8 i = 0; i < block_len; ++i) _state[i] = sbox[_state[i]];
}

void aes_base::inv_sub_bytes(uint_8* _state) {
    for (uint_8 i = 0; i < block_len; ++i) _state[i] = rsbox[_state[i]];
}

void aes_base::shift_rows(uint_8* _state) {
    state_type& state = *(state_type*) _state;

    uint_8 temp = state[0][1];
    state[0][1] = state[1][1];
    state[1][1] = state[2][1];
    state[2][1] = state[3][1];
    state[3][1] = temp;

    temp = state[0][2];
    state[0][2] = state[2][2];
    state[2][2] = temp;
    temp = state[1][2];
    state[1][2] = state[3][2];
    state[3][2] = temp;

    temp = state[0][3];
    state[0][3] = state[3][3];
    state[3][3] = state[2][3];
    state[2][3] = state[1][3];
    state[1][3] = temp;
}

void aes_base::inv_shift_rows(uint_8* _state) {
    state_type& state = *(state_type*) _state;

    uint8_t temp = state[3][1];
    state[3][1] = state[2][1];
    state[2][1] = state[1][1];
    state[1][1] = state[0][1];
    state[0][1] = temp;

    temp = state[0][2];
    state[0][2] = state[2][2];
    state[2][2] = temp;
    temp = state[1][2];
    state[1][2] = state[3][2];
    state[3][2] = temp;

    temp = state[0][3];
    state[0][3] = state[1][3];
    state[1][3] = state[2][3];
    state[2][3] = state[3][3];
    state[3][3] = temp;
}

void aes_base::mix_columns(uint_8* _state) {
    state_type& state = *(state_type*) _state;
    uint_8 tmp, t;

    for (uint_8 i = 0; i < block_num; ++i) {
        t = state[i][0];
        tmp = state[i][0] ^ state[i][1] ^ state[i][2] ^ state[i][3];
        state[i][0] ^= xtime(state[i][0] ^ state[i][1]) ^ tmp;
        state[i][1] ^= xtime(state[i][1] ^ state[i][2]) ^ tmp;
        state[i][2] ^= xtime(state[i][2] ^ state[i][3]) ^ tmp;
        state[i][3] ^= xtime(state[i][3] ^ t) ^ tmp;
    }
}

void aes_base::inv_mix_columns(uint_8* _state) {
    state_type& state = *(state_type*) _state;
    uint_8 a, b, c, d;

    for (uint_8 i = 0; i < block_num; i++) {
        a = state[i][0];
        b = state[i][1];
        c = state[i][2];
        d = state[i][3];

        state[i][0] = multiply(a, 0x0e) ^ multiply(b, 0x0b) ^ multiply(c, 0x0d) ^ multiply(d, 0x09);
        state[i][1] = multiply(a, 0x09) ^ multiply(b, 0x0e) ^ multiply(c, 0x0b) ^ multiply(d, 0x0d);
        state[i][2] = multiply(a, 0x0d) ^ multiply(b, 0x09) ^ multiply(c, 0x0e) ^ multiply(d, 0x0b);
        state[i][3] = multiply(a, 0x0b) ^ multiply(b, 0x0d) ^ multiply(c, 0x09) ^ multiply(d, 0x0e);
    }
}

inline uint_8 aes_base::xtime(uint_8 _x) {
    return ((_x << 1) ^ ((_x & 0x80) ? 0x1b : 0x00));
}

inline uint_8 aes_base::multiply(uint_8 _x, uint_8 _y) {
    return (((_y & 0x01) ? _x : 0x00) ^
            ((_y & 0x02) ? xtime(_x) : 0x00) ^
            ((_y & 0x04) ? xtime(xtime(_x)) : 0x00) ^
            ((_y & 0x08) ? xtime(xtime(xtime(_x))) : 0x00) ^
            ((_y & 0x10) ? xtime(xtime(xtime(xtime(_x)))) : 0x00));
}

void alx::aes_base::ghash(uint_8* _s, const uint_8* _h, const uint_8* _d) {
    is_pclmulqdq_supported_v ? ghash_hard(_s, _h, _d) : ghash_soft(_s, _h, _d);
}

void aes_base::increment_counter(uint_8* _counter) {
    for (int i = block_len - 1; i >= 0; --i)
        if (++_counter[i]) break;
}

void alx::aes_base::ghash_soft(uint_8* _s, const uint_8* _h, const uint_8* _d) {
    for (int i = 0; i < block_len; ++i) _s[i] ^= _d[i];
    ghash_mul_soft(_s, _h);
}

void alx::aes_base::ghash_mul_soft(uint_8* _a, const uint_8* _b) {
    uint_8 V[16], R[16]{0};
    memcpy(V, _b, block_len);
    for (int i = 0; i < block_bits_len; ++i) {
        int byte_idx = i >> 3;
        int bit_idx = 7 - (i & 7);
        if ((_a[byte_idx] >> bit_idx) & 1)
            for (int j = 0; j < block_len; ++j) R[j] ^= V[j];
        bool lsb = V[15] & 1;
        for (int j = 15; j > 0; --j) V[j] = (V[j] >> 1) | ((V[j - 1] & 1) << 7);
        V[0] >>= 1;
        // 0XE1 is R = 11100001 followed by 15 zero bytes, the GCM field's reduction constant
        if (lsb) V[0] ^= 0XE1U;
    }
    memcpy(_a, R, block_len);
}

void alx::aes_base::ghash_hard(uint_8* _s, const uint_8* _h, const uint_8* _d) {
#if TRY_AES_HARD
    _mm_storeu_si128((__m128i*) _s, _mm_xor_si128(_mm_loadu_si128((const __m128i*) _s),
                                                  _mm_loadu_si128((const __m128i*) _d)));
    ghash_mul_hard(_s, _h);
#endif
}

void alx::aes_base::ghash_mul_hard(uint_8* _a, const uint_8* _b) {
#if TRY_AES_HARD
    __m128i product = ghash_mul_reflected(ghash_reverse_bits(_mm_loadu_si128((const __m128i*) _a)),
                                          ghash_reverse_bits(_mm_loadu_si128((const __m128i*) _b)));
    _mm_storeu_si128((__m128i*) _a, ghash_reverse_bits(product));
#endif
}