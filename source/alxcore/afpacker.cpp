/*****************************************************************/ /**
 * \file   afpacker.cpp
 * \brief  File system packing and unpacking tool (supports optional encryption and compression)
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "afpacker.h"

#include "aaes.h"
#include "astream_ex.h"
#include "astring.h"
#include "avarsolid.h"
#include "averify.h"

using namespace alx;
using namespace alx::fpacker;

constexpr uint_32 FILE_HEAD = 0X5F415A4CU;
constexpr uint_32 FILE_TAIL = 0X5F455942U;

constexpr uint_32 FILE_CMPS = 0X5F535043U;
constexpr uint_32 FILE_AESC = 0X5F534541U;
constexpr uint_32 FILE_NONE = 0X00000000U;

constexpr verify::COMMON_TYPE CHECK_TYPE = verify::CRC_32C;

#define RETURN_ERROR(THIS, PATH, ERR)                                         \
    {                                                                         \
        if (nullptr != THIS->print_rtmsg_) THIS->print_rtmsg_(PATH, -1, ERR); \
        return ERR;                                                           \
    }
#define ABORT_CHECK(THIS, PATH)                                                             \
    {                                                                                       \
        if (nullptr != THIS->abort_ && *THIS->abort_) RETURN_ERROR(THIS, PATH, forceabort); \
    }

encoder::encoder(bool _cmps, const std::string& _pswd, uint_64 _blck_size)
    : cmps_(_cmps ? new compress::encoder_lz4() : nullptr), ostm_(nullptr), size_(_blck_size), pswd_(_pswd.empty() ? bytes() : bytes::from_hex(verify::exec(verify::SHA_256, _pswd))) {
}

encoder::~encoder() {
    finish();
    delete cmps_;
    delete ostm_;
}

bool encoder::start(ostream* _ostm) {
    if (!finish()) return false;
    ostm_ = _ostm;
    ostm_->append_ordinary(FILE_HEAD);
    ostm_->append_ordinary(nullptr == cmps_ ? FILE_NONE : FILE_CMPS);
    ostm_->append_ordinary(pswd_.empty() ? FILE_NONE : FILE_AESC);
    fsys_.clear();
    return true;
}

bool encoder::finish() {
    if (nullptr == ostm_) return true;
    variant* error{nullptr};
    bytes fsys_data = varsolid::to_bytes(fsys_, &error);
    if (nullptr != error) return false;

    // {plain size, stored size, index start}: the trailer takes the first two, the last two are the IV
    uint_64 fsys_size[3]{fsys_data.size(), 0, ostm_->total()};
    if (nullptr != cmps_) {
        cmps_->clear();
        fsys_data = cmps_->exec(fsys_data);
        if (fsys_data.empty()) return false;
    }
    fsys_size[1] = fsys_data.size();

    if (!pswd_.empty())
        aes_ctr<256>(pswd_.data(), (const uint_8*) (fsys_size + 1)).encrypt(fsys_data);

    return ostm_->append(fsys_data) &&
                   ostm_->append_ordinary(fsys_size[0]) &&
                   ostm_->append_ordinary(fsys_size[1]) &&
                   ostm_->append_ordinary(FILE_TAIL) &&
                   ostm_->flush()
               ? (safe_delete(ostm_), true)
               : false;
}

uint_8 encoder::append(const file_info& _finf, const std::string& _name) {
    return append_impl(_finf, _name, this, fsys_);
}

const char* encoder::error_message(uint_8 _err) {
    switch (_err) {
    case notexist: return "file_not_exist";
    case linkfile: return "unsupport_link_file";
    case duppath: return "duplicate_path";
    case readfail: return "read_failed";
    case cmpsfail: return "compression_failed";
    case flushfail: return "flush_failed";
    case forceabort: return "force_abort";
    default: return "unknown";
    }
}

uint_8 encoder::append_impl(const file_info& _finf, const std::string& _name, encoder* _this, varmap& _root) {
    if (_finf.is_link()) {
        if (nullptr != _this->print_rtmsg_) _this->print_rtmsg_(_finf.path(), -1, linkfile);
        return _this->igerr_linkfile_ ? success : linkfile;
    }
    if (!_finf.is_exist()) {
        if (nullptr != _this->print_rtmsg_) _this->print_rtmsg_(_finf.path(), -1, notexist);
        return _this->igerr_notexist_ ? success : notexist;
    }
    return (_finf.is_dir() ? append_impl_dir : append_impl_file)(_finf, _name, _this, _root);
}

uint_8 encoder::append_impl_dir(const file_info& _finf, const std::string& _name, encoder* _this, varmap& _root) {
    varmap dir_root;
    for (const auto& info : _finf.get_child()) {
        uint_8 ret = append_impl(info, info.name(), _this, dir_root);
        if (ret != success) return ret;
    }

    _root.insert(_name, dir_root);
    return success;
}

uint_8 encoder::append_impl_file(const file_info& _finf, const std::string& _name, encoder* _this, varmap& _root) {
    ABORT_CHECK(_this, _finf.path());
    if (_root.contain(_name)) RETURN_ERROR(_this, _finf.path(), duppath);
    file fs;
    if (!fs.open(_finf, file::READ)) RETURN_ERROR(_this, _finf.path(), readfail);

    uint_64 offset{0};
    bytes data;
    varvec blocks;
    // cut the LZ4 chain at every file boundary; the decoder cuts at the same one
    if (nullptr != _this->cmps_) _this->cmps_->clear();

    // comma chain, not a comparison: read the block, advance past it, stop when the read came back empty
    while (data = fs.read(offset, _this->size_), offset += data.size(), !data.empty()) {
        ABORT_CHECK(_this, _finf.path());
        std::vector<uint_64> block_mesgs;

        // one block is two index entries: {plain size, stored size, archive offset} then the CRC of the plain bytes
        block_mesgs.push_back(data.size());
        const std::string block_check = verify::exec(CHECK_TYPE, data);

        if (nullptr != _this->cmps_) {
            data = _this->cmps_->exec(data);
            if (data.empty()) RETURN_ERROR(_this, _finf.path(), cmpsfail);
        }
        block_mesgs.push_back(data.size());
        block_mesgs.push_back(_this->ostm_->total());

        // IV: the 16 bytes from element [1] on -- stored size and archive offset -- both read back from the index
        if (!_this->pswd_.empty()) aes_ctr<256>(_this->pswd_.data(), (const uint_8*) (block_mesgs.data() + 1)).encrypt(data);
        if (!_this->ostm_->append(data)) RETURN_ERROR(_this, _finf.path(), flushfail);

        blocks.push_back(block_mesgs);
        blocks.push_back(block_check);

        if (nullptr != _this->print_rtmsg_) _this->print_rtmsg_(_finf.path(), _finf.size(), offset);
    }
    if (offset != _finf.size()) RETURN_ERROR(_this, _finf.path(), readfail);

    // closes the vec with the entry's total plain size, after the pairs
    blocks.push_back(offset);
    _root.insert(_name, blocks);
    return success;
}

bool decoder::is_encrypt(const istream& _istm) {
    bytes_view d = _istm.get(8, 4);
    return d.empty() ? false : FILE_AESC == r_interpret<uint_32>(d.data());
}

bool decoder::is_compress(const istream& _istm) {
    bytes_view d = _istm.get(4, 4);
    return d.empty() ? false : FILE_CMPS == r_interpret<uint_32>(d.data());
}

decoder::decoder(istream* _istm, const std::string& _pswd)
    : istm_(_istm), pswd_(_pswd.empty() ? bytes() : bytes::from_hex(verify::exec(verify::SHA_256, _pswd))) {
    parse_head();
    parse_tail();
}

decoder::~decoder() {
    delete cmps_;
    delete istm_;
}

uint_8 decoder::get(const std::string& _root, const std::list<std::string>& _path, bytes& _odat) const {
    return fsys_.contain(_root) ? get_impl(this, fsys_.value(_root).select(_path, variant::def_val()), _odat) : notexist;
}

uint_8 decoder::get(const std::string& _root, const std::list<std::string>& _path, ostream& _ostm) const {
    return fsys_.contain(_root) ? get_impl(this, fsys_.value(_root).select(_path, variant::def_val()), _ostm) : notexist;
}

uint_8 decoder::save(const std::string& _root, const std::list<std::string>& _path, const file_info& _dir) const {
    if (_root.empty() || _root == "*") return save_impl_dir(this, fsys_, _dir, std::string());
    else if (fsys_.contain(_root)) return save_impl(this, fsys_.value(_root).select(_path, variant::def_val()), _dir, _path.empty() ? _root : _path.back());
    else return notexist;
}

const char* decoder::error_message(uint_8 _err) {
    switch (_err) {
    case success: return "success";
    case notexist: return "file_not_exist";
    case notfile: return "not_a_file";
    case needdir: return "target_must_be_directory";
    case badfsys: return "corrupted_file_system";
    case badblock: return "corrupted_block";
    case readfail: return "read_failed";
    case writefail: return "write_failed";
    case cdirfail: return "create_directory_failed";
    case dcmpsfail: return "decompression_failed";
    case flushfail: return "flush_failed";
    case forceabort: return "force_abort";
    default: return "unknown";
    }
}

bool decoder::parse_head() {
    bytes_view data = istm_->get(0, 12);
    if (data.size() != 12) return false;
    const uint_32* head = (const uint_32*) data.cdata();

    if (FILE_HEAD != head[0]) return false;
    switch (head[1]) {
    case FILE_CMPS: cmps_ = new compress::decoder_lz4(); break;
    case FILE_NONE: break;
    default: return false;
    }
    switch (head[2]) {
    case FILE_AESC: break;
    case FILE_NONE: pswd_.clear(); break;
    default: return false;
    }
    return true;
}

bool decoder::parse_tail() {
    // a stream shorter than 20 bytes wraps this offset; get() clamps it and the size check below rejects the read
    bytes_view data = istm_->get(istm_->total() - 20, 20);
    if (data.size() != 20) return false;

    const uint_64 cmps_size = m_interpret<uint_64>(data.cdata() + sizeof(uint_64));
    const uint_64 fsys_size[3]{
        m_interpret<uint_64>(data.cdata()),
        cmps_size,
        istm_->total() - 20 - cmps_size};

    if (FILE_TAIL != m_interpret<uint_32>(data.data() + 16)) return false;
    bytes fsys_data = istm_->get(istm_->total() - 20 - fsys_size[1], fsys_size[1]).to_bytes();
    if (fsys_data.size() != fsys_size[1]) return false;

    if (!pswd_.empty())
        aes_ctr<256>(pswd_.data(), (const uint_8*) (fsys_size + 1)).decrypt(fsys_data);

    if (nullptr != cmps_) {
        fsys_data = cmps_->exec(fsys_data, (uint_32) fsys_size[0]);
        if (fsys_data.empty()) return false;
    }

    return varsolid::to_varmap(fsys_data, fsys_);
}

uint_8 decoder::get_impl(const decoder* _this, const variant& _ctrl, bytes& _odat) {
    ostream_buff ostm(_odat);
    return _odat.clear(), get_impl(_this, _ctrl, ostm);
}

uint_8 decoder::get_impl(const decoder* _this, const variant& _ctrl, ostream& _ostm) {
    ABORT_CHECK(_this, _ostm.name());
    if (_ctrl.null()) RETURN_ERROR(_this, _ostm.name(), notexist);
    if (!_ctrl.is<varvec>()) RETURN_ERROR(_this, _ostm.name(), notfile);

    varvec __blocks = _ctrl.to<varvec>();
    uint_64 __offset{0};
    // pairs plus the trailing total: the count is odd and at least three
    if (__blocks.size() < 3 || (__blocks.size() & 0X01U) == 0) RETURN_ERROR(_this, _ostm.name(), badblock);
    uint_64 __total = __blocks.back().to<uint_64>();
    // the caller's stream may already hold bytes, so the totals below are measured from here
    const uint_64 ostm_ofst = _ostm.total();
    if (nullptr != _this->cmps_) _this->cmps_->clear();

    while (__offset < __blocks.size() - 1) {
        ABORT_CHECK(_this, _ostm.name());
        const std::vector<uint_64> block_mesgs = __blocks[__offset++].to<std::vector<uint_64>>();
        const std::string block_check = __blocks[__offset++].to_string();
        if (block_mesgs.size() < 3 || block_check.empty()) RETURN_ERROR(_this, _ostm.name(), badblock);

        bytes data = _this->istm_->get(block_mesgs[2], block_mesgs[1]).to_bytes();
        if (data.size() != block_mesgs[1]) RETURN_ERROR(_this, _ostm.name(), readfail);

        if (!_this->pswd_.empty())
            aes_ctr<256>(_this->pswd_.data(), (const uint_8*) (block_mesgs.data() + 1)).decrypt(data);

        if (nullptr != _this->cmps_) {
            data = _this->cmps_->exec(data, (uint_32) block_mesgs[0]);
            if (data.empty()) RETURN_ERROR(_this, _ostm.name(), dcmpsfail);
        }

        if (verify::exec(CHECK_TYPE, data) != block_check) RETURN_ERROR(_this, _ostm.name(), badblock);
        if (!_ostm.append(data)) RETURN_ERROR(_this, _ostm.name(), flushfail);

        if (nullptr != _this->print_rtmsg_) _this->print_rtmsg_(_ostm.name(), __total, _ostm.total() - ostm_ofst);
    }
    if (_ostm.total() - ostm_ofst != __total) RETURN_ERROR(_this, _ostm.name(), readfail);
    if (!_ostm.flush()) RETURN_ERROR(_this, _ostm.name(), flushfail);

    return success;
}

uint_8 decoder::save_impl(const decoder* _this, const variant& _fsys, const file_info& _dir, const std::string& key) {
    const std::string _key = strutil::code_conver(key, strutil::locale_format());
    if (!_dir.is_exist() || !_dir.is_dir()) RETURN_ERROR(_this, _dir.path(), needdir);
    if (_fsys.is<varmap>()) return save_impl_dir(_this, _fsys, _dir, _key);
    else if (_fsys.is<varvec>()) return save_impl_file(_this, _fsys, _dir, _key);
    else RETURN_ERROR(_this, _dir.path(), badfsys);
}

uint_8 decoder::save_impl_dir(const decoder* _this, const variant& _fsys, const file_info& _dir, const std::string& _key) {
#ifdef _WIN32
    file_info child = _key.empty() ? _dir : file_info(strutil::format("%1\\%2", _dir.path(), _key));
#else
    file_info child = _key.empty() ? _dir : file_info(strutil::format("%1/%2", _dir.path(), _key));
#endif
    if (!child.mkdir()) RETURN_ERROR(_this, _dir.path(), cdirfail);
    varmap vmap = _fsys.to<varmap>();
    for (const auto& it : vmap) {
        uint_8 ret = save_impl(_this, *it.second, child, it.first);
        if (success != ret) return ret;
    }
    return success;
}

uint_8 decoder::save_impl_file(const decoder* _this, const variant& _ctrl, const file_info& _dir, const std::string& _key) {
#ifdef _WIN32
    ostream_file ostm(file_info(strutil::format("%1\\%2", _dir.path(), _key)), false);
#else
    ostream_file ostm(file_info(strutil::format("%1/%2", _dir.path(), _key)), false);
#endif
    return get_impl(_this, _ctrl, ostm);
}
