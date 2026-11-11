/*****************************************************************/ /**
 * \file   afpacker_v2.h
 * \brief  The v2 archive envelope — AES-256-GCM, key derivation head, sealed index
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#ifndef _ALEXIS_FPACKER_V2_H_
#define _ALEXIS_FPACKER_V2_H_

#include "afpacker_base.h"

namespace alx {
    namespace fpacker {
        namespace detail {

            /// v2's cipher word: AES-256-GCM; the common head's compress flag still applies
            constexpr uint_32 ARC_AESG = 0X47534541U;
            /// Bytes of the salt in the version head
            constexpr uint_64 ARC_AESG_SALT_SIZE = 16;
            /// Bytes the version head adds after the common one: cipher, kdf id, iterations, salt
            constexpr uint_64 ARC_AESG_HEAD_SIZE = sizeof(uint_32) + 1 + sizeof(uint_32) + ARC_AESG_SALT_SIZE;
            /// Byte offsets inside the version head
            constexpr uint_64 ARC_AESG_ITER_OFF = sizeof(uint_32) + 1;
            constexpr uint_64 ARC_AESG_SALT_OFF = ARC_AESG_ITER_OFF + sizeof(uint_32);
            /// Bytes of the whole header, the AAD prefix of every sealed region
            constexpr uint_64 ARC_AESG_HEAD_LEN = ARC_COMMON_HEAD_SIZE + ARC_AESG_HEAD_SIZE;
            /// Bytes of a block's or the index's authentication tag
            constexpr uint_64 ARC_AESG_TAG_SIZE = 16;
            /// Bytes of the trailer: index plain size, index stored size, then ARC_TAIL
            constexpr uint_64 ARC_AESG_TAIL_SIZE = 2 * sizeof(uint_64) + sizeof(uint_32);
            /// Smallest block: below it the tag and the per-block work stop being negligible
            constexpr uint_64 ARC_AESG_MIN_BLCK = 0X00010000U;
            /// Iteration count of the key derivation: the default, and the largest accepted
            constexpr uint_32 ARC_AESG_DEF_ITER = 0X00400000U;
            constexpr uint_32 ARC_AESG_MAX_ITER = 0X01000000U;
            /// KDF id 1 is PBKDF2-HMAC-SHA256; 0 and everything else are invalid
            constexpr uint_8 ARC_AESG_KDF_PBKDF2 = 1;
            /// Nonce kinds: 1 a data block, 2 the index; 0 is invalid
            constexpr uint_32 ARC_AESG_KIND_BLOCK = 1;
            constexpr uint_32 ARC_AESG_KIND_INDEX = 2;
            /// Bytes of the key every region is sealed under
            constexpr uint_64 ARC_AESG_KEY_SIZE = 32;

            /// v2 writer: one derived key, every block and the index sealed with AES-GCM
            class encoder_v2 : public encoder_base {
            public:
                encoder_v2(bool _cmps, uint_64 _blck_size, const std::string& _pswd, uint_32 _iterations);

            private:
                bool write_head(ostream* _ostm) override;
                uint_8 write_block(const bytes& _plain, uint_64 _offset, bytes& _stored, varvec& _blocks) override;
                bool write_meta(ostream* _ostm, const varmap& _archive, const varmap& _fsys) override;

            private:
                std::string pswd_;
                uint_32 iterations_;
                bytes head_;
                bytes key_;
            };

            /// v2 reader: derives the key, verifies the index, then every block
            class decoder_v2 : public decoder_base {
            public:
                decoder_v2(istream* _istm, const std::string& _pswd);

                /// True when the common header's cipher word is the v2 one
                static bool is_accept(const istream& _istm);

                bool is_encrypt() const override;

            private:
                uint_8 read_meta(varmap& _archive, varmap& _fsys) override;
                uint_8 read_block(const varvec& _blocks, uint_64& _pos, bytes& _plain) const override;

            private:
                std::string pswd_;
                bytes head_;
                bytes key_;
            };

        }
    }
}

#endif
