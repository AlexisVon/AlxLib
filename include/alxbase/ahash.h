/*****************************************************************/ /**
 * \file   ahash.h
 * \brief  Hash family: auto_64 (wyhash by length, ah_64 above 64 B), ah_64, wy_64 and FNV-1a
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef _ALEXIS_HASH_H_
#define _ALEXIS_HASH_H_

#include "abase.h"
#include "autility.h"

#include <cstring>
#include <type_traits>

namespace alx {
    namespace hash {

        /**
         * \brief Two layers: xx_yy hash sources, and hash::func as the constraint/adapter
         *
         * Every xx_yy class (yy = output width) exposes exactly two primitives:
         *   hash(const void*, size_t) and hash(uint_64),
         * plus is_hard() and is_constexpr(); the class declares no return_type -- the wrapper
         * hash::func<Hash, Ret> fixes the return type and checks the primitives answer it.
         * A class whose is_constexpr() is true also carries a constexpr hash(const char*).
         * Input extension (NUL-terminated text, arithmetic types by bit pattern, any type with
         * data()/size()) belongs to the wrapper hash::func<Hash, Ret>::eval, not to the class.
         * An implementation swaps in with one alias at the call site:
         *   using hash = alx::hash::ah_64;
         */

        /**
         * \brief Runtime input wrapper: fixes the hash return type, checks the primitives answer
         * it, and extends the input range (NUL-terminated text, arithmetic by bit pattern, any
         * type with data()/size() -- std::string and bytes included). A type outside those
         * ranges fails here, not in the map.
         *
         * The checks fire when func<Hash, Ret> is instantiated, which is always outside the hash
         * class definition, so Hash is complete. Ret is the caller's declaration of the answer;
         * the decltype comparisons verify the primitives match it.
         */
        template <typename Hash, typename Ret>
        struct func {
            using return_type = Ret;

            static_assert(std::is_same<decltype(Hash::hash((const void*) 0, size_t(0))), Ret>::value,
                          "func: byte primitive must answer Ret");
            static_assert(std::is_same<decltype(Hash::hash(uint_64(0))), Ret>::value,
                          "func: integer primitive must answer Ret");

            static return_type eval(const char* _s) noexcept { return Hash::hash(_s, std::strlen(_s)); }

            template <typename T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>
            static return_type eval(T _x) noexcept {
                static_assert(sizeof(T) <= 8, "hash supports up to 64-bit keys");
                uint_64 bits = 0;
                std::memcpy(&bits, &_x, sizeof(T));
                return Hash::hash(bits);
            }

            template <typename T, typename std::enable_if<has_data_size<T>::value, int>::type = 0>
            static return_type eval(const T& _v) noexcept {
                return Hash::hash(_v.data(), _v.size());
            }
        };

        /// 64-bit non-cryptographic hash; the hardware path lives in ahash.cpp
        class ALXBASE_API ah_64 {
        public:
            ah_64() = delete;

            static uint_64 hash(const void* _data, size_t _size) noexcept;
            static uint_64 hash(uint_64 _x) noexcept;
            static bool is_hard() noexcept;
            static constexpr bool is_constexpr() noexcept { return false; }
        };

        /**
         * \brief 64-bit wyhash (Wang Yi's final v4.3 construction; public domain), entirely inline
         *
         * No CPU dispatch and no .so call: every call site compiles the mixing inline. The portable
         * product path keeps the answers identical on compilers without a 128-bit integer type.
         * The two primitive forms are independent, the same way ah_64's are.
         */
        class ALXBASE_API wy_64 {
        public:
            wy_64() = delete;

            static uint_64 hash(const void* _data, size_t _size) noexcept {
                const uint_8* p = static_cast<const uint_8*>(_data);
                uint_64 seed = mix(S0, S1);
                uint_64 a, b;
                if (_size <= 16) {
                    if (_size >= 4) {
                        a = (read4(p) << 32) | read4(p + ((_size >> 3) << 2));
                        b = (read4(p + _size - 4) << 32) | read4(p + _size - 4 - ((_size >> 3) << 2));
                    } else if (_size > 0) {
                        a = read3(p, _size);
                        b = 0;
                    } else {
                        a = b = 0;
                    }
                } else {
                    size_t i = _size;
                    if (i >= 48) {
                        uint_64 see1 = seed;
                        uint_64 see2 = seed;
                        do {
                            seed = mix(read8(p) ^ S1, read8(p + 8) ^ seed);
                            see1 = mix(read8(p + 16) ^ S2, read8(p + 24) ^ see1);
                            see2 = mix(read8(p + 32) ^ S3, read8(p + 40) ^ see2);
                            p += 48;
                            i -= 48;
                        } while (i >= 48);
                        seed ^= see1 ^ see2;
                    }
                    while (i > 16) {
                        seed = mix(read8(p) ^ S1, read8(p + 8) ^ seed);
                        i -= 16;
                        p += 16;
                    }
                    a = read8(p + i - 16);
                    b = read8(p + i - 8);
                }
                a ^= S1;
                b ^= seed;
                mum(a, b);
                return mix(a ^ S0 ^ (uint_64) _size, b ^ S1);
            }
            static uint_64 hash(uint_64 _x) noexcept {
                uint_64 a = _x ^ S0;
                uint_64 b = S1;
                mum(a, b);
                return mix(a ^ S0, b ^ S1);
            }
            static constexpr bool is_hard() noexcept { return false; }
            static constexpr bool is_constexpr() noexcept { return false; }

        private:
            static constexpr uint_64 S0 = 0x2D358DCCAA6C78A5ULL;
            static constexpr uint_64 S1 = 0x8BB84B93962EACC9ULL;
            static constexpr uint_64 S2 = 0x4B33A62ED433D4A3ULL;
            static constexpr uint_64 S3 = 0x4D5A2DA51DE1AA47ULL;

            /// The 64x64 -> 128 product, low half in _a and high half in _b
            static void mum(uint_64& _a, uint_64& _b) noexcept {
#if defined(__SIZEOF_INT128__)
                const __uint128_t r = (__uint128_t) _a * _b;
                _a = (uint_64) r;
                _b = (uint_64) (r >> 64);
#else
                const uint_64 ha = _a >> 32, hb = _b >> 32, la = (uint_32) _a, lb = (uint_32) _b;
                const uint_64 rh = ha * hb, rm0 = ha * lb, rm1 = hb * la, rl = la * lb;
                const uint_64 t = rl + (rm0 << 32);
                uint_64 c = t < rl;
                const uint_64 lo = t + (rm1 << 32);
                c += lo < t;
                _a = lo;
                _b = rh + (rm0 >> 32) + (rm1 >> 32) + c;
#endif
            }
            static uint_64 mix(uint_64 _a, uint_64 _b) noexcept {
                mum(_a, _b);
                return _a ^ _b;
            }
            static uint_64 read8(const uint_8* _p) noexcept {
                uint_64 v;
                std::memcpy(&v, _p, 8);
                return v;
            }
            static uint_64 read4(const uint_8* _p) noexcept {
                uint_32 v;
                std::memcpy(&v, _p, 4);
                return v;
            }
            static uint_64 read3(const uint_8* _p, size_t _k) noexcept {
                return ((uint_64) _p[0] << 16) | ((uint_64) _p[_k >> 1] << 8) | _p[_k - 1];
            }
        };

        /**
         * \brief 64-bit automatic dispatch: wyhash through 64 B, ah_64 above
         *
         * The split follows the measured crossover (`tools/ahash_bench.cpp`): wy_64 wins or ties
         * through 64 B, ah_64's AES path wins from 65 B up (1.2-1.65x from 127 B). Keys of 64 B or
         * less answer CPU-independently; a longer key answers whatever ah_64's effective path
         * answers, and without AES compiled in that path is slower than wy_64 -- correctness is
         * unaffected, the pair is only ever compared inside one process. is_hard() reports the
         * long-key branch. The integer primitive routes to wy_64's inline mix.
         */
        class ALXBASE_API auto_64 {
        public:
            auto_64() = delete;

            static uint_64 hash(const void* _data, size_t _size) noexcept {
                return _size <= WY_MAX ? wy_64::hash(_data, _size) : ah_64::hash(_data, _size);
            }
            static uint_64 hash(uint_64 _x) noexcept { return wy_64::hash(_x); }
            static bool is_hard() noexcept { return ah_64::is_hard(); }
            static constexpr bool is_constexpr() noexcept { return false; }

        private:
            static constexpr size_t WY_MAX = 64;
        };

        /// 32-bit FNV-1a; the offset basis is fixed, hash(const char*) is the constexpr entry
        class ALXBASE_API fnv_32 {
        public:
            fnv_32() = delete;

            static constexpr uint_32 hash(const char* _s) noexcept { return walk(_s, OFFSET); }
            static uint_32 hash(const void* _data, size_t _size) noexcept;
            static uint_32 hash(uint_64 _x) noexcept;
            static constexpr bool is_hard() noexcept { return false; }
            static constexpr bool is_constexpr() noexcept { return true; }

        private:
            static constexpr uint_32 OFFSET = 0x811C9DC5U;
            static constexpr uint_32 PRIME = 0x01000193U;

            static constexpr uint_32 walk(const char* _s, uint_32 _h) noexcept {
                return *_s ? walk(_s + 1, (_h ^ static_cast<uint_32>(static_cast<uint_8>(*_s))) * PRIME) : _h;
            }
        };

        /// 64-bit FNV-1a; same shape as fnv_32
        class ALXBASE_API fnv_64 {
        public:
            fnv_64() = delete;

            static constexpr uint_64 hash(const char* _s) noexcept { return walk(_s, OFFSET); }
            static uint_64 hash(const void* _data, size_t _size) noexcept;
            static uint_64 hash(uint_64 _x) noexcept;
            static constexpr bool is_hard() noexcept { return false; }
            static constexpr bool is_constexpr() noexcept { return true; }

        private:
            static constexpr uint_64 OFFSET = 0xCBF29CE484222325ULL;
            static constexpr uint_64 PRIME = 0x00000100000001B3ULL;

            static constexpr uint_64 walk(const char* _s, uint_64 _h) noexcept {
                return *_s ? walk(_s + 1, (_h ^ static_cast<uint_64>(static_cast<uint_8>(*_s))) * PRIME) : _h;
            }
        };

    }
}

#endif
