/*****************************************************************/ /**
 * \file   afpacker_v1.h
 * \brief  The v1 archive envelope — AES-256-CTR, bare tree index, 12-byte header
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#ifndef _ALEXIS_FPACKER_V1_H_
#define _ALEXIS_FPACKER_V1_H_

#include "afpacker_base.h"

namespace alx {
    namespace fpacker {
        namespace detail {

            /// v1's cipher word: AES-256-CTR; ARC_NONE still means a plain v1 archive
            constexpr uint_32 ARC_AESC = 0X5F534541U;

            /// v1 writer: the fast, legacy-strength profile; the key is a single SHA-256
            class encoder_v1 : public encoder_base {
            public:
                encoder_v1(bool _cmps, uint_64 _blck_size, const std::string& _pswd);

            private:
                bool write_head(ostream* _ostm) override;
                uint_8 write_block(const bytes& _plain, uint_64 _offset, bytes& _stored, varvec& _blocks) override;
                bool write_meta(ostream* _ostm, const varmap& _archive, const varmap& _fsys) override;

            private:
                bytes key_;
            };

            /// v1 reader: decrypts the CTR envelope; the tree index is the whole index
            class decoder_v1 : public decoder_base {
            public:
                decoder_v1(istream* _istm, const std::string& _pswd);

                /// True when the common header's cipher word is one this version understands
                static bool is_accept(const istream& _istm);

                bool is_encrypt() const override;

            private:
                uint_8 read_meta(varmap& _archive, varmap& _fsys) override;
                uint_8 read_block(const varvec& _blocks, uint_64& _pos, bytes& _plain) const override;

            private:
                bytes key_;
                bool encrypt_{false};
            };

        }
    }
}

#endif
