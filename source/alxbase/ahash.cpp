/*****************************************************************/ /**
 * \file   ahash.cpp
 * \brief  ah_64 scalar path, FNV-1a runtime overloads
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "ahash.h"
#include "autility.h"

#if (defined(__AES__) || defined(_MSC_VER)) && !defined(ALX_AHASH_NO_HW)
#    define ALX_AHASH_HW 1
#else
#    define ALX_AHASH_HW 0
#endif

#if ALX_AHASH_HW
#    if defined(_MSC_VER)
#        include <intrin.h>
#    else
#        include <cpuid.h>
#    endif
#    include <wmmintrin.h>
#endif

// Merged into ah_64::hash so the single exported call is the only boundary left
#if defined(_MSC_VER)
#    define ALX_AHASH_INL __forceinline
#else
#    define ALX_AHASH_INL inline __attribute__((always_inline))
#endif

namespace alx {
    namespace hash {
        namespace ah_64_impl {
            constexpr uint_64 K1 = 0x9E3779B97F4A7C15ULL;
            constexpr uint_64 K2 = 0xC2B2AE3D27D4EB4FULL;
            constexpr uint_64 K3 = 0x165667B19E3779F9ULL;
            constexpr uint_64 SEED = 0x243F6A8885A308D3ULL;

            ALX_AHASH_INL uint_64 rotl(uint_64 _x, int _r) noexcept { return (_x << _r) | (_x >> (64 - _r)); }

            ALX_AHASH_INL uint_64 avalanche(uint_64 _h) noexcept {
                _h ^= _h >> 33;
                _h *= K2;
                _h ^= _h >> 29;
                _h *= K3;
                _h ^= _h >> 32;
                return _h;
            }

            ALX_AHASH_INL uint_64 hash_soft(const void* _data, size_t _size) noexcept {
                const uint_8* p = static_cast<const uint_8*>(_data);
                uint_64 h = SEED ^ ((uint_64) _size * K1);
                if (_size >= 8) {
                    if (_size <= 16) {
                        h ^= rotl(m_interpret<uint_64>(p) * K2, 31) ^ rotl(m_interpret<uint_64>(p + _size - 8) * K3, 17);
                    } else {
                        uint_64 a = h ^ K1;
                        uint_64 b = h ^ K2;
                        for (size_t i = 0; i + 16 <= _size; i += 16) {
                            a = rotl((a ^ m_interpret<uint_64>(p + i)) * K2, 27);
                            b = rotl((b ^ m_interpret<uint_64>(p + i + 8)) * K3, 31);
                        }
                        a ^= m_interpret<uint_64>(p + _size - 16);
                        b ^= m_interpret<uint_64>(p + _size - 8);
                        h ^= rotl(a * K2, 23) ^ (b * K3);
                    }
                } else if (_size >= 4) {
                    uint_64 v = ((uint_64) m_interpret<uint_32>(p) << 32) | m_interpret<uint_32>(p + _size - 4);
                    h ^= rotl(v, 23) * K2;
                } else if (_size > 0) {
                    uint_64 v = (uint_64) p[0] | ((uint_64) p[_size >> 1] << 8) | ((uint_64) p[_size - 1] << 16);
                    h ^= v * K3;
                }
                return avalanche(h);
            }

            ALX_AHASH_INL uint_64 hash_int(uint_64 _x) noexcept { return avalanche(_x * K1 + K2); }

            ALX_AHASH_INL bool cpu_has_aes() noexcept {
#if ALX_AHASH_HW
                static const bool s_hard = []() {
#    if defined(_MSC_VER)
                    int regs[4];
                    __cpuid(regs, 1);
                    return (regs[2] & (1 << 25)) != 0;
#    else
                    unsigned int eax, ebx, ecx, edx;
                    if (0 == __get_cpuid(1, &eax, &ebx, &ecx, &edx)) return false;
                    return (ecx & (1u << 25)) != 0;
#    endif
                }();
                return s_hard;
#else
                return false;
#endif
            }

#if ALX_AHASH_HW
            ALX_AHASH_INL uint_64 hash_hard(const void* _data, size_t _size) noexcept {
                const uint_8* p = static_cast<const uint_8*>(_data);
                const __m128i c1 = _mm_set_epi64x((long long) K2, (long long) K3);
                const __m128i c2 = _mm_set_epi64x((long long) SEED, (long long) K1);
                __m128i x;
                if (_size >= 8) {
                    x = _mm_set_epi64x((long long) m_interpret<uint_64>(p + _size - 8),
                                       (long long) m_interpret<uint_64>(p));
                } else if (_size >= 4) {
                    x = _mm_set_epi64x((long long) m_interpret<uint_32>(p + _size - 4),
                                       (long long) m_interpret<uint_32>(p));
                } else if (_size > 0) {
                    const uint_64 v = (uint_64) p[0] | ((uint_64) p[_size >> 1] << 8) | ((uint_64) p[_size - 1] << 16);
                    x = _mm_set_epi64x(0, (long long) v);
                } else {
                    x = _mm_setzero_si128();
                }

                __m128i s = _mm_set_epi64x((long long) SEED, (long long) ((uint_64) _size * K1));
                if (_size <= 16) {
                    // Data enters as the AES round input, not as the XOR key: one round spreads a
                    // byte over its column, three rounds spread it over the whole block.
                    s = _mm_aesenc_si128(_mm_xor_si128(s, x), c1);
                    s = _mm_aesenc_si128(s, c2);
                    s = _mm_aesenc_si128(s, c1);
                } else {
                    __m128i s0 = s;
                    __m128i s1 = _mm_xor_si128(s, c2);
                    size_t i = 0;
                    for (; i + 32 <= _size; i += 32) {
                        s0 = _mm_aesenc_si128(_mm_loadu_si128(reinterpret_cast<const __m128i*>(p + i)), s0);
                        s1 = _mm_aesenc_si128(_mm_loadu_si128(reinterpret_cast<const __m128i*>(p + i + 16)), s1);
                    }
                    if (i + 16 <= _size) s0 = _mm_aesenc_si128(_mm_loadu_si128(reinterpret_cast<const __m128i*>(p + i)), s0);
                    const __m128i tail = _mm_loadu_si128(reinterpret_cast<const __m128i*>(p + _size - 16));
                    s0 = _mm_aesenc_si128(tail, s0);
                    s1 = _mm_aesenc_si128(tail, s1);
                    s = _mm_xor_si128(s0, s1);
                    s = _mm_aesenc_si128(s, c1);
                    s = _mm_aesenc_si128(s, c2);
                }
                return (uint_64) _mm_cvtsi128_si64(s);
            }
#else
            /// No AES path compiled in: have_hard() answers false and the branch folds away
            ALX_AHASH_INL uint_64 hash_hard(const void* _data, size_t _size) noexcept { return hash_soft(_data, _size); }
#endif

            ALX_AHASH_INL bool have_hard() noexcept { return cpu_has_aes(); }
        }

        uint_64 ah_64::hash(const void* _data, size_t _size) noexcept {
            return ah_64_impl::have_hard() ? ah_64_impl::hash_hard(_data, _size) : ah_64_impl::hash_soft(_data, _size);
        }

        uint_64 ah_64::hash(uint_64 _x) noexcept { return ah_64_impl::hash_int(_x); }

        bool ah_64::is_hard() noexcept { return ah_64_impl::have_hard(); }

        uint_32 fnv_32::hash(const void* _data, size_t _size) noexcept {
            uint_32 h = OFFSET;
            const uint_8* p = static_cast<const uint_8*>(_data);
            for (size_t i = 0; i < _size; i++) h = (h ^ p[i]) * PRIME;
            return h;
        }

        uint_32 fnv_32::hash(uint_64 _x) noexcept {
            uint_8 b[8];
            for (int i = 0; i < 8; i++) b[i] = (uint_8) (_x >> (8 * i));
            return hash(b, 8);
        }

        uint_64 fnv_64::hash(const void* _data, size_t _size) noexcept {
            uint_64 h = OFFSET;
            const uint_8* p = static_cast<const uint_8*>(_data);
            for (size_t i = 0; i < _size; i++) h = (h ^ p[i]) * PRIME;
            return h;
        }

        uint_64 fnv_64::hash(uint_64 _x) noexcept {
            uint_8 b[8];
            for (int i = 0; i < 8; i++) b[i] = (uint_8) (_x >> (8 * i));
            return hash(b, 8);
        }

    }
}
