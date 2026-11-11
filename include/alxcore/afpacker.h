/*****************************************************************/ /**
 * \file   afpacker.h
 * \brief  File system packing and unpacking tool (supports optional encryption and compression)
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#ifndef _ALEXIS_FPACKER_H_
#define _ALEXIS_FPACKER_H_

#include "aaes.h"
#include "acompress.h"
#include "afile.h"
#include "astream.h"
#include "autility.h"
#include "avariant.h"

namespace alx {
    namespace fpacker {

        namespace detail {
            struct encoder_base;
            struct decoder_base;
        }

        /**
         * \brief Progress and error callback, shared by encoder and decoder
         *
         * _v1 == (uint_64) -1 reports a failure: _v2 is the uint_8 code that the call is about to
         * return. Any other _v1 is progress: _v1 is the total size of the entry being processed
         * and _v2 how much of it is done. _path is the source path when packing; when unpacking
         * it is the name of the output stream -- the target file for save(), empty for a memory
         * buffer.
         *
         * The encoder also reports two creation-time codes through this slot: randfallback is a
         * warning (the OS entropy source failed, the local mix was used and packing continues),
         * randfail is a failure (no usable salt; start() returns false).
         */
        typedef std::function<void(const std::string&, uint_64, uint_64)> MesgFunc;
        /**
         * \brief Write a packed archive: the file data in blocks, then an index of the packed tree
         *
         * Instances come from create() (plain v1), create_encrypt_v1() (encrypted v1) or
         * create_encrypt_v2() (authenticated), and the caller deletes them. The factories never
         * throw: a bad parameter returns null with the code in _err.
         *
         * Lifecycle: start() takes the stream, append() adds entries, finish() writes the index
         * and releases the stream; the destructor finishes an open archive. The encoder owns the
         * stream from start() on.
         *
         * This class is a shell: every call forwards to a detail::encoder_base, which owns the
         * traversal, the compression state and the index.
         */
        class ALXCORE_API encoder : public noncopyable {
        private:
            explicit encoder(detail::encoder_base* _impl);

        public:
            /// Result codes of append() and of the factories; error_message() turns one into text.
            /// The factory writes success or badparams; randfallback and randfail only travel through MesgFunc.
            enum : uint_8 {
                /// The call did what it was asked to
                success = 0,
                /// The source path does not exist
                notexist,
                /// The source path is a symbolic link; links are never packed
                linkfile,
                /// A name is already taken at that level of the index
                duppath,
                /// The source could not be opened, or its size changed while it was read
                readfail,
                /// LZ4 returned nothing for a block
                cmpsfail,
                /// The output stream refused a write
                flushfail,
                /// The abort flag was raised; the archive is left incomplete
                forceabort,
                /// A creation parameter is out of range
                badparams,
                /// The OS entropy source failed; a local-mix salt was used and packing continues
                randfallback,
                /// The OS entropy source failed and the fallback was not accepted; packing stopped
                randfail,
            };

            /// Finish an open archive as finish() does, then delete the stream and the encoder
            ~encoder();

            /**
             * \brief Create an encoder for a plain (unencrypted) v1 archive
             *
             * \param _cmps Compress the data blocks and the index with LZ4
             * \param _err When not null, receives success, or badparams when _blck_size is 0 or
             *             4 GiB and above
             * \param _blck_size Plain bytes of one file per block, default 64 MiB; 0 and 4 GiB
             *                   and above are refused
             * \return A new encoder owned by the caller, or null
             */
            static encoder* create(bool _cmps, uint_8* _err = nullptr, uint_64 _blck_size = 0X04000000U);

            /**
             * \brief Create an encoder for an encrypted v1 archive
             *
             * This is the fast, legacy-strength profile: deterministic and unauthenticated. The
             * same password yields the same key, so use a fresh one for each archive; use it only
             * where that trade-off is acceptable.
             *
             * \param _pswd Password; empty is refused
             * \param _cmps Compress the data blocks and the index with LZ4
             * \param _err When not null, receives success, or badparams for an empty password, a
             *             zero block size or 4 GiB and above
             * \param _blck_size Plain bytes of one file per block, default 64 MiB; 0 and 4 GiB
             *                   and above are refused
             * \return A new encoder owned by the caller, or null
             */
            static encoder* create_encrypt_v1(const std::string& _pswd, bool _cmps,
                                              uint_8* _err = nullptr,
                                              uint_64 _blck_size = 0X04000000U);

            /**
             * \brief Create an encoder for an authenticated (v2) archive
             *
             * One key per archive, derived from the password and a fresh random salt; every block
             * and the index are authenticated. The salt comes from the OS entropy source; when
             * that fails, the encoder either falls back to the local mix (reported as randfallback
             * through the callback) or stops with randfail, depending on
             * set_igerr_randfallback().
             *
             * \param _pswd Password; empty is refused
             * \param _cmps Compress the data blocks and the index with LZ4
             * \param _err When not null, receives success, or badparams for an empty password, a
             *             block size below 64 KiB or 4 GiB and above, or an iteration count
             *             outside 1..16 Mi
             * \param _blck_size Plain bytes of one file per block, default 64 MiB; below 64 KiB
             *                   and 4 GiB and above are refused
             * \param _iterations Key-derivation rounds per archive; default 4 Mi, maximum 16 Mi
             * \return A new encoder owned by the caller, or null
             */
            static encoder* create_encrypt_v2(const std::string& _pswd, bool _cmps,
                                              uint_8* _err = nullptr,
                                              uint_64 _blck_size = 0X04000000U,
                                              uint_32 _iterations = 0X00400000U);

            /**
             * \brief Archive-level metadata as a caller-owned map, written into the index
             *
             * The caller may insert any keys between start() and finish(); the map is cleared by
             * start(). A v1 writer ignores it.
             */
            varmap& archive();

            /**
             * \brief Open a new archive on a stream, taking ownership of it
             *
             * Writes the format header and clears the index. An archive already open is finished
             * first.
             *
             * \param _ostm Output stream; deleted by finish() or by the destructor
             * \return false when a previously opened archive could not be finished -- the new
             *         stream is then not taken -- or when the header could not be written, in
             *         which case the encoder already owns the stream
             */
            bool start(ostream* _ostm);

            /**
             * \brief Write the index and close the archive
             *
             * A no-op returning true when no archive is open, so a second call is harmless. On
             * success the stream is deleted; on failure it stays with the encoder and the
             * destructor deletes it. An archive holding no entry at all is refused, because the
             * index format cannot represent one.
             *
             * \return false when the archive holds no entry, or the index cannot be serialized or
             *         compressed, or cannot be written and flushed
             */
            bool finish();

            /**
             * \brief Add one file or one directory tree to the archive
             *
             * Valid only between start() and finish(). A file is cut into blocks of at most
             * _blck_size plain bytes, compressed and encrypted as configured. A directory is
             * walked recursively into a nested index entry; the first failing child drops the
             * whole subtree, while the blocks already written stay in the stream, unreferenced.
             *
             * \param _finf Source file or directory
             * \param _name Name the entry takes in the index, i.e., the root name used by get()
             * \return success, or the first failure encountered; a missing source or a symbolic
             *         link still returns success when the matching ignore flag is set, though the
             *         message callback reports it either way
             */
            uint_8 append(const file_info& _finf, const std::string& _name);

            /// The stream passed to start(); null before start() and after a successful finish()
            const ostream* get_stream() const;
            /**
             * \brief Abort packing on an external flag
             *
             * The flag is read before every file and every block, so raising it costs at most one
             * block. It must outlive the calls that may read it; null disables the check.
             *
             * \param _abort_flag Caller-owned flag; packing stops once *flag is true
             */
            void set_abort_flag(const bool* _abort_flag);
            /// Install the progress callback; null, the default, reports nothing
            void set_print_rtmsg(MesgFunc _fc);
            /// True when a missing source is skipped instead of failing the append
            bool is_igerr_notexist() const;
            /// Skip a missing source: the callback still reports notexist, append() returns success
            void set_igerr_notexist(bool _ig);
            /// True when a symbolic link is skipped instead of failing the append
            bool is_igerr_linkfile() const;
            /// Skip a symbolic link: the callback still reports linkfile, append() returns success
            void set_igerr_linkfile(bool _ig);
            /// True when a failed OS entropy source may fall back to the local mix (v2 only)
            bool is_igerr_randfallback() const;
            /// Accept the fallback: the callback reports randfallback and packing continues
            void set_igerr_randfallback(bool _ig);

        public:
            /**
             * \brief Text for a result code
             *
             * \return A static string, never null; success and every unrecognized code give
             *         "unknown"
             */
            static const char* error_message(uint_8 _err);

        private:
            detail::encoder_base* impl_;
        };

        /**
         * \brief Read a packed archive: header and index at creation, file data on demand
         *
         * Instances come from create() and are deleted by the caller. create() reads the header
         * and the index, so the stream must support reading at an arbitrary offset; the decoder
         * owns and deletes it. There is no separate validity flag: a stream that cannot be opened
         * makes create() return null.
         *
         * This class is a shell: every call forwards to a detail::decoder_base, which owns the
         * traversal, the compression state and the index.
         */
        class ALXCORE_API decoder : public noncopyable {
        private:
            explicit decoder(detail::decoder_base* _impl);

        public:
            /// Result codes of get() and save(); error_message() turns one into text
            enum : uint_8 {
                /// The call did what it was asked to
                success = 0,
                /// No such root in the index, or the path resolved to nothing
                notexist,
                /// The path resolved to a directory where a file was wanted
                notfile,
                /// The save target is missing, or is not a directory
                needdir,
                /// The selected entry is neither a directory nor a file
                badfsys,
                /// A block record is malformed, or a block failed its integrity check
                badblock,
                /// A block could not be read in full, or the entry's size does not add up
                readfail,
                /// Never produced by this version; a failed write surfaces as flushfail
                writefail,
                /// A directory below the save target could not be created
                cdirfail,
                /// LZ4 could not decompress a block or the index
                dcmpsfail,
                /// The output stream refused a write or a flush
                flushfail,
                /// The abort flag was raised; the target may hold a partial entry
                forceabort,
                /// The stream is not an archive this build knows, or its head is not valid
                badversion,
                /// The head is recognized but a parameter in it is not supported
                badparams,
                /// An authenticated region failed its tag check
                badauth,
            };

            /**
             * \brief What this build makes of a stream's header, as create() reads it
             *
             * When no family claims the stream every field is zero.
             */
            struct alignas(uint_32) pktype {
                /// A family this build has claims the head
                bool clan;
                /// The common head marks it compressed
                bool cmps;
                /// A password is needed to read it
                bool encrypt;
                /// 1 or 2; 0 when no family claims the stream
                uint_8 version;

                /// The four fields as one word
                uint_32& valid() { return r_interpret<uint_32>(this); }
                const uint_32& valid() const { return r_interpret<const uint_32>(this); }
            };

            static_assert(sizeof(pktype) == sizeof(uint_32), "pktype must fit one word");

            /// Delete the stream
            ~decoder();

            /**
             * \brief Open an archive from a stream, taking ownership of it
             *
             * Picks the matching reader from the header and parses the index. The stream is owned
             * from the call on: it is deleted by the returned decoder, and also when the call
             * fails.
             *
             * \param _istm Stream to own; deleted by the returned decoder and on failure
             * \param _pswd Password; empty means the archive carries no key
             * \param _err When not null, receives success on success and the failure code
             *             otherwise -- badversion, badparams, badauth, readfail, dcmpsfail, badfsys
             * \return A decoder owned by the caller, or null when the stream cannot be read
             */
            static decoder* create(istream* _istm, const std::string& _pswd, uint_8* _err = nullptr);

            /**
             * \brief Classify a stream from its header alone, without opening it
             *
             * The one header reader behind create(): the common head is checked first, then each
             * version from the newest down, the first to accept winning. Every field is zero when
             * no family claims the stream.
             *
             * \param _istm Stream to inspect; read positionally, so it is left as it was
             */
            static pktype probe(const istream& _istm);

            /**
             * \brief Archive-level metadata as it came out of the index
             *
             * Nested varmaps and scalars as the writer left them. A v1 archive gives an empty
             * map. Valid for the life of the decoder.
             */
            const varmap& archive() const;

            /**
             * \brief The packed tree as it was indexed
             *
             * Nested varmaps for directories, one varvec of block records per file -- the shape
             * that get() and save() walk with a path. Valid for the life of the decoder.
             */
            const varmap& fsystem() const;

        public:
            /**
             * \brief Read one file out of the archive into memory
             *
             * Every block named by the index is read, decrypted, decompressed and checked before
             * its bytes are appended to _odat, so what lands there is plain data. _odat is
             * cleared first, and keeps what was decoded before a failing block.
             *
             * \param _root Root name handed to encoder::append()
             * \param _path Names below that root; empty selects _root itself
             * \param _odat Filled with the file content
             * \return success, or the first failure among notexist, notfile, badblock, readfail,
             *         dcmpsfail, badauth, flushfail and forceabort
             */
            uint_8 get(const std::string& _root, const std::list<std::string>& _path, bytes& _odat) const;

            /**
             * \brief Read one file out of the archive into an output stream
             *
             * Same reads and checks as the bytes form. The stream is only appended to -- never
             * rewound or truncated -- and flushed once the entry is complete.
             *
             * \param _root Root name handed to encoder::append()
             * \param _path Names below that root; empty selects _root itself
             * \param _ostm Destination, written at its current position
             * \return As the bytes form
             */
            uint_8 get(const std::string& _root, const std::list<std::string>& _path, ostream& _ostm) const;

            /**
             * \brief Extract one entry, or the whole archive, onto disk
             *
             * With _root empty or "*" the whole index is written under _dir and _path is ignored.
             * Otherwise _root must be in the index, and the entry its _path selects becomes
             * _dir/_path.back(), or _dir/_root when _path is empty. Names are converted to the
             * local charset on the way out, files are opened for write -- an existing target is
             * truncated -- and the directories below _dir are created as needed; _dir itself must
             * already exist.
             *
             * \param _root Root name, or empty / "*" for the whole archive
             * \param _path Names below that root; the last one names the extracted entry
             * \param _dir Existing directory to extract into
             * \return success, or needdir, notexist, cdirfail, badfsys and the get() codes
             */
            uint_8 save(const std::string& _root, const std::list<std::string>& _path, const file_info& _dir) const;

            /**
             * \brief Abort extraction on an external flag
             *
             * The flag is read before every block, so raising it costs at most one block. It must
             * outlive the calls that may read it; null disables the check.
             *
             * \param _abort_flag Caller-owned flag; extraction stops once *flag is true
             */
            void set_abort_flag(const bool* _abort_flag);
            /// The stream the factory was given
            const istream* get_stream() const;
            /// Whether this archive is marked as encrypted, read again from its header
            bool is_encrypt() const;
            /// Whether this archive is marked as compressed, read again from its header
            bool is_compress() const;
            /// Install the progress callback; null, the default, reports nothing
            void set_print_rtmsg(MesgFunc _fc);

        public:
            /**
             * \brief Text for a result code
             *
             * \return A static string, never null; success gives "success", every unrecognized
             *         code gives "unknown"
             */
            static const char* error_message(uint_8 _err);

        private:
            detail::decoder_base* impl_;
        };
    }
}

#endif
