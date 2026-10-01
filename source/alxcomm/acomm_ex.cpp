/*****************************************************************/ /**
 * \file   acomm_ex.cpp
 * \brief  xtended communication functionality (supports varmap data structure and compressed transmission)
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "acomm_ex.h"
#include "acompress.h"
#include "avarsolid.h"
#include "averify.h"

using namespace alx;

namespace {
    constexpr uint_64 _PACK_HEAD_ = 0X0123456789ABCDEFU;
    constexpr uint_64 _PACK_TAIL_ = 0XFEDCBA9876543210U;

    // written to the wire verbatim (append_ordinary): this layout and its byte order are the protocol
    struct pack_head {
        struct SIZE {
        public:
            // set_size() writes both halves, so a corrupted or never-written size fails here
            inline bool valid() const { return size_ == ~anti_; }
            inline uint_64 size() const { return size_; }
            inline void set_size(uint_64 _value) {
                size_ = _value;
                anti_ = ~_value;
            }

        private:
            uint_64 size_{0};
            uint_64 anti_{0};
        } orig, cmps;
        struct CTRL {
            union {
                uint_8 data[4];
                uint_32 value{0};
            } version;
            union {
                uint_8 data[4];
                uint_32 value{0};
            } checksum;
            union {
                uint_8 data[8];
                uint_64 value{0};
            } extra[3];
        } ctrl;

        bool valid() const { return orig.size() != 0 && cmps.size() != 0 && orig.valid() && cmps.valid(); }

#define EXTRA_FLAG(NAME, ROW, COL)                            \
    void set_##NAME(bool _true) { set_bit<ROW, COL>(_true); } \
    bool is_##NAME() const { return get_bit<ROW, COL>(); }

        EXTRA_FLAG(compress, 0, 0);
        EXTRA_FLAG(verify, 0, 1);

        static std::pair<bool, uint_32> checksum(const bytes_view& _data) {
            verify* crc32c = verify::create(verify::CRC_32C);
            if (nullptr == crc32c || !crc32c->hardcal()) {
                delete crc32c;
                return {false, 0};
            }
            crc32c->update(_data);
            bytes value = crc32c->bytedigest();
            delete crc32c;
            return {true, r_interpret<uint_32>(value.data())};
        }

    private:
        template <uint_8 _row, uint_8 _col>
        bool get_bit() const {
            static_assert(_row < 3 && _col < 8, "get_bit() out of range");
            return bit_get(ctrl.extra->data + _row, _col);
        }
        template <uint_8 _row, uint_8 _col>
        void set_bit(bool _true) {
            static_assert(_row < 3 && _col < 8, "set_bit() out of range");
            bit_set(ctrl.extra->data + _row, _col, _true);
        }
    };

    constexpr uint_64 HEAD_SIZE = sizeof(_PACK_HEAD_) + sizeof(pack_head);
    constexpr uint_64 TAIL_SIZE = sizeof(_PACK_TAIL_);
    constexpr uint_64 EMPTY_SIZE = HEAD_SIZE + TAIL_SIZE;

    class packer : public noncopyable {
    public:
        packer(signal<const std::string&>& _log) : print_mesg(_log) {}

    public:
        static bytes encoder(const varmap& _data, bool _compress, bool _crc32c) {
            bytes orig_data = varsolid::to_bytes(_data);
            // an empty result is the failure signal -- data_send() logs it and sends nothing
            if (orig_data.empty()) return orig_data;

            bytes encoder;
            bytes cmps_data = _compress ? compress::encoder_lz4::sexec(orig_data) : orig_data;

            pack_head block;
            block.orig.set_size(orig_data.size());
            block.cmps.set_size(cmps_data.size());
            block.ctrl.version.value = 0x00000001U;
            block.set_compress(_compress);
            if (_crc32c) {
                const std::pair<bool, uint_32> checksum = pack_head::checksum(cmps_data);
                if (checksum.first) {
                    block.set_verify(true);
                    block.ctrl.checksum.value = checksum.second;
                }
            }

            encoder.append_ordinary(_PACK_HEAD_);
            encoder.append_ordinary(block);
            encoder.append(cmps_data);
            encoder.append_ordinary(_PACK_TAIL_);

            return encoder;
        }
        static bool decoder(const bytes_view& _data, varmap& _value, std::string& _error) {
            // no payload byte means no valid pack, and this bound keeps the reads below in range
            if (_data.size() <= EMPTY_SIZE) {
                _error = "pack too small";
                return false;
            }
            if (m_interpret<uint_64>(_data.data()) != _PACK_HEAD_ ||
                m_interpret<uint_64>(_data.data() + _data.size() - sizeof(_PACK_TAIL_)) != _PACK_TAIL_) {
                _error = "error pack border";
                return false;
            }
            const pack_head head = m_interpret<pack_head>(_data.data() + sizeof(_PACK_HEAD_));
            if (!head.valid()) {
                _error = "invalid pack head";
                return false;
            }
            if (head.cmps.size() + EMPTY_SIZE != _data.size()) {
                _error = "error data size";
                return false;
            }

            bytes_view data = _data.mid_view(HEAD_SIZE, head.cmps.size());
            if (head.is_verify()) {
                const std::pair<bool, uint_32> checksum = pack_head::checksum(data);
                // no hardware CRC available means this check is skipped, not failed
                if (checksum.first && checksum.second != head.ctrl.checksum.value) {
                    _error = "checksum error";
                    return false;
                }
            }
            data = head.is_compress() ? compress::decoder_lz4::sexec(data, (int_32) head.orig.size()) : data;
            if (data.size() != head.orig.size()) {
                _error = "compress error";
                return false;
            }

            _value = varsolid::to_varmap(data);
            _error.clear();
            return true;
        }

    public:
        std::list<bytes> append(const bytes_view& _data) {
            m_cache.append(_data);
            uint_64 head{0};
            std::list<bytes> result;

            // streaming reassembly: an incomplete tail stays cached for the next call, so this may return
            // nothing at all; a head that fails its checks is stepped over, a headless cache dropped whole
            while (m_cache.size() > EMPTY_SIZE) {
                head = m_cache.find_ordinary(_PACK_HEAD_, 0);
                if (head == uint_64_npos) {
                    print_mesg.exec("no pack head in cache, drop all!");
                    m_cache.clear();
                    break;
                }

                if (m_cache.size() <= head + EMPTY_SIZE) {
                    m_cache = m_cache.mid(head);
                    break;
                }

                const pack_head block = m_interpret<pack_head>(m_cache.data() + head + sizeof(_PACK_HEAD_));
                if (!block.valid()) {
                    print_mesg.exec("find head, but size error, drop head!");
                    m_cache = m_cache.mid(head + HEAD_SIZE);
                    continue;
                }
                if (m_cache.size() < head + EMPTY_SIZE + block.cmps.size()) {
                    m_cache = m_cache.mid(head);
                    break;
                }
                if (m_interpret<uint_64>(m_cache.data() + head + HEAD_SIZE + block.cmps.size()) != _PACK_TAIL_) {
                    print_mesg.exec("find head and size, but tail error, drop head!");
                    m_cache = m_cache.mid(head + HEAD_SIZE);
                    continue;
                }

                result.push_back(m_cache.mid(head, EMPTY_SIZE + block.cmps.size()));
                m_cache = m_cache.mid(head + EMPTY_SIZE + block.cmps.size());
            }

            return result;
        }
        void clear() { m_cache.clear(); }

    private:
        bytes m_cache;
        signal<const std::string&>& print_mesg;
    };
}

// same convention as bytes_send(): the caller serializes the sends -- nothing here enforces that
bool alx::comm_ex::data_send(const varmap& _data, uint_64 _loc, bool _compress) {
    if (!is_valid()) return false;
    if (_data.empty()) return true;

    bytes pack = packer::encoder(_data, _compress, true);
    if (pack.empty()) {
        mesg_prit.exec("pack varmap failed");
        return false;
    }
    return bytes_send(pack, _loc);
}

void alx::comm_ex::on_bytes_recv(const bytes_view& _data, uint_64 _loc) {
    // shared per thread: the cache and the log reference belong to the first comm_ex that received here
    static thread_local packer decoder(mesg_prit);
    std::list<bytes> packs = decoder.append(_data);
    for (const bytes& pack : packs) {
        varmap value;
        std::string errstr;
        if (packer::decoder(pack, value, errstr))
            data_recv.exec(value, _loc);
        else
            mesg_prit.exec("decode failed, errstr: " + errstr);
    }
}
