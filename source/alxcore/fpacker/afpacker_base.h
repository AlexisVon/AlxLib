/*****************************************************************/ /**
 * \file   afpacker_base.h
 * \brief  Shared archive engine; a version product produces every block, the index and the trailer
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#ifndef _ALEXIS_FPACKER_BASE_H_
#define _ALEXIS_FPACKER_BASE_H_

#include "acompress.h"
#include "afpacker.h"

namespace alx {
    namespace fpacker {
        namespace detail {

            /// The common header every archive opens with: the family magic and the compress flag.
            /// Everything past it -- the cipher word and any version payload -- belongs to the version.
            constexpr uint_32 ARC_HEAD = 0X5F415A4CU;
            constexpr uint_32 ARC_CMPS = 0X5F535043U;
            constexpr uint_32 ARC_NONE = 0X00000000U;
            constexpr uint_32 ARC_TAIL = 0X5F455942U;
            constexpr uint_32 ARC_COMMON_HEAD_SIZE = 2 * sizeof(uint_32);
            /// Largest plain block: the read side takes a block's plain size through 32 bits
            constexpr uint_64 ARC_MAX_BLCK = 0XFFFFFFFFU;

            /**
             * \brief The shared write engine: tree walk, file I/O, LZ4, progress, abort
             *
             * One object owns every piece of state a format switch could desynchronize; a product
             * produces every block and its index entries and the index block in its own shape.
             * The common header and the trailing marker belong to this base. The public encoder
             * is a shell forwarding here.
             */
            struct encoder_base : public noncopyable {
            protected:
                /**
                 * \param _cmps Compress blocks and the index with LZ4
                 * \param _blck_size Plain bytes per block; a product may floor or refuse it
                 */
                encoder_base(bool _cmps, uint_64 _blck_size);

            public:
                virtual ~encoder_base();

                /// True when this archive compresses with LZ4
                bool is_compress() const;
                /// Archive-level metadata; cleared by start() and handed to write_meta()
                varmap& archive();

                /// As encoder::start()
                bool start(ostream* _ostm);
                /// As encoder::finish()
                bool finish();
                /// As encoder::append()
                uint_8 append(const file_info& _finf, const std::string& _name);

                void set_abort_flag(const bool* _abort_flag);
                const ostream* get_stream() const;
                void set_print_rtmsg(MesgFunc _fc);
                bool is_igerr_notexist() const;
                void set_igerr_notexist(bool _ig);
                bool is_igerr_linkfile() const;
                void set_igerr_linkfile(bool _ig);
                bool is_igerr_randfallback() const;
                void set_igerr_randfallback(bool _ig);

            protected:
                /// Report a code through the installed callback as _v1 == -1, _v2 == _code
                void report_code(const std::string& _path, uint_8 _code) const;
                /// Reset the LZ4 chain before a file or before the index
                void reset_compress();
                /// Compress _data in place when the archive compresses; false when LZ4 fails
                bool compress_block(bytes& _data);

            private:
                /// Write this version's head after the common one: the cipher word and any payload
                virtual bool write_head(ostream* _ostm) = 0;
                /**
                 * \brief Produce one block and its index entries
                 *
                 * The engine hands over the plain bytes and the archive offset the block starts
                 * at; the product compresses, seals, appends whatever entries its record shape
                 * needs and returns the stored bytes, which the engine writes out.
                 */
                virtual uint_8 write_block(const bytes& _plain, uint_64 _offset, bytes& _stored, varvec& _blocks) = 0;
                /// Serialize, seal and write the index and its sizes in this version's shape
                virtual bool write_meta(ostream* _ostm, const varmap& _archive, const varmap& _fsys) = 0;

            private:
                static uint_8 append_impl(const file_info& _finf, const std::string& _name, encoder_base* _this, varmap& _root);
                static uint_8 append_impl_dir(const file_info& _finf, const std::string& _name, encoder_base* _this, varmap& _root);
                static uint_8 append_impl_file(const file_info& _finf, const std::string& _name, encoder_base* _this, varmap& _root);

            private:
                compress::encoder_lz4* cmps_;
                ostream* ostm_;
                uint_64 size_;
                varmap fsys_;
                varmap archive_;

                const bool* abort_{nullptr};
                MesgFunc print_rtmsg_;
                bool igerr_notexist_{false};
                bool igerr_linkfile_{false};
                bool igerr_randfallback_{false};
            };

            /**
             * \brief The shared read engine: index traversal, file I/O, LZ4, save, progress, abort
             *
             * A product parses the header and the index in its own shape and consumes its own
             * block records. The public decoder is a shell forwarding here.
             */
            struct decoder_base : public noncopyable {
            protected:
                explicit decoder_base(istream* _istm);

                /// Create or drop the LZ4 decoder to match the header
                void enable_compress(bool _cmps);
                /// Reset the LZ4 chain before a file
                void reset_compress() const;
                /// Decompress _data to _plain_size bytes when the archive compresses; false on failure
                bool decompress_block(bytes& _data, uint_64 _plain_size) const;

            public:
                virtual ~decoder_base();

                /// As decoder::probe(): the one header reader create() and the public face share
                static decoder::pktype probe(const istream& _istm);

                /**
                 * \brief Build the decoder for a stream
                 *
                 * probe() picks the family from the header, the newest down, and its open() then
                 * runs.
                 *
                 * \param _istm Stream taken over on success, deleted on failure
                 * \param _pswd Password handed to the version
                 * \param _err Receives the failure code when null is returned
                 * \return The opened decoder, or null
                 */
                static decoder_base* create(istream* _istm, const std::string& _pswd, uint_8* _err);

                /// True when the stream carries this family's common header: magic and compress flag
                static bool is_accept(const istream& _istm);

                /// Parse the head and the index; create() calls it once, after construction
                uint_8 open();

                /// Whether this archive is marked as encrypted, read from the version header
                virtual bool is_encrypt() const = 0;
                /// Whether this archive is marked as compressed, read from its header
                bool is_compress() const;

                /// The archive-level metadata parsed out of the index
                const varmap& archive() const;
                /// The packed tree as it was indexed
                const varmap& fsystem() const;
                uint_8 get(const std::string& _root, const std::list<std::string>& _path, bytes& _odat) const;
                uint_8 get(const std::string& _root, const std::list<std::string>& _path, ostream& _ostm) const;
                uint_8 save(const std::string& _root, const std::list<std::string>& _path, const file_info& _dir) const;

                void set_abort_flag(const bool* _abort_flag);
                const istream* get_stream() const;
                void set_print_rtmsg(MesgFunc _fc);

            private:
                /// Parse the header and the index in this version's shape, deriving the key
                virtual uint_8 read_meta(varmap& _archive, varmap& _fsys) = 0;
                /// Consume this version's record(s) at _pos, unseal and check one block
                virtual uint_8 read_block(const varvec& _blocks, uint_64& _pos, bytes& _plain) const = 0;

            private:
                static uint_8 get_impl(const decoder_base* _this, const variant& _ctrl, bytes& _odat);
                static uint_8 get_impl(const decoder_base* _this, const variant& _ctrl, ostream& _ostm);
                static uint_8 save_impl(const decoder_base* _this, const variant& _fsys, const file_info& _dir, const std::string& _key);
                static uint_8 save_impl_dir(const decoder_base* _this, const variant& _fsys, const file_info& _dir, const std::string& _key);
                static uint_8 save_impl_file(const decoder_base* _this, const variant& _ctrl, const file_info& _dir, const std::string& _key);

            private:
                compress::decoder_lz4* cmps_{nullptr};
                istream* istm_;
                varmap fsys_;
                varmap archive_;

                const bool* abort_{nullptr};
                MesgFunc print_rtmsg_;
            };

        }
    }
}

#endif
