/*****************************************************************/ /**
 * \file   afpacker_base.cpp
 * \brief  Shared archive engine — traversal, LZ4, progress, abort
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "afpacker_base.h"
#include "afpacker_v1.h"
#include "afpacker_v2.h"

#include "astream_ex.h"
#include "astring.h"
#include "autility.h"

using namespace alx;
using namespace alx::fpacker;
using namespace alx::fpacker::detail;


#define RETURN_ERROR(THIS, PATH, ERR)                                         \
    {                                                                         \
        if (nullptr != THIS->print_rtmsg_) THIS->print_rtmsg_(PATH, -1, ERR); \
        return ERR;                                                           \
    }
#define ABORT_CHECK(THIS, PATH, ERR)                                                 \
    {                                                                                \
        if (nullptr != THIS->abort_ && *THIS->abort_) RETURN_ERROR(THIS, PATH, ERR); \
    }

encoder_base::encoder_base(bool _cmps, uint_64 _blck_size)
    : cmps_(_cmps ? new compress::encoder_lz4() : nullptr), ostm_(nullptr), size_(_blck_size) {
}

encoder_base::~encoder_base() {
    delete cmps_;
    delete ostm_;
}

bool encoder_base::is_compress() const { return nullptr != cmps_; }

varmap& encoder_base::archive() { return archive_; }

bool encoder_base::start(ostream* _ostm) {
    if (!finish()) return false;
    ostm_ = _ostm;
    fsys_.clear();
    archive_.clear();
    return _ostm->append_ordinary(ARC_HEAD) &&
           _ostm->append_ordinary(nullptr != cmps_ ? ARC_CMPS : ARC_NONE) &&
           write_head(_ostm);
}

bool encoder_base::finish() {
    if (nullptr == ostm_) return true;
    // varsolid cannot parse a root map with no entries, so an unreadable archive is refused here
    if (fsys_.empty()) return false;
    return write_meta(ostm_, archive_, fsys_) && ostm_->append_ordinary(ARC_TAIL) && ostm_->flush() ? (safe_delete(ostm_), true) : false;
}

uint_8 encoder_base::append(const file_info& _finf, const std::string& _name) {
    return append_impl(_finf, _name, this, fsys_);
}

void encoder_base::set_abort_flag(const bool* _abort_flag) { abort_ = _abort_flag; }

const ostream* encoder_base::get_stream() const { return ostm_; }

void encoder_base::set_print_rtmsg(MesgFunc _fc) { print_rtmsg_ = _fc; }

bool encoder_base::is_igerr_notexist() const { return igerr_notexist_; }

void encoder_base::set_igerr_notexist(bool _ig) { igerr_notexist_ = _ig; }

bool encoder_base::is_igerr_linkfile() const { return igerr_linkfile_; }

void encoder_base::set_igerr_linkfile(bool _ig) { igerr_linkfile_ = _ig; }

bool encoder_base::is_igerr_randfallback() const { return igerr_randfallback_; }

void encoder_base::set_igerr_randfallback(bool _ig) { igerr_randfallback_ = _ig; }

void encoder_base::report_code(const std::string& _path, uint_8 _code) const {
    if (nullptr != print_rtmsg_) print_rtmsg_(_path, (uint_64) -1, _code);
}

void encoder_base::reset_compress() {
    if (nullptr != cmps_) cmps_->clear();
}

bool encoder_base::compress_block(bytes& _data) {
    if (nullptr == cmps_) return true;
    _data = cmps_->exec(_data);
    return !_data.empty();
}

uint_8 encoder_base::append_impl(const file_info& _finf, const std::string& _name, encoder_base* _this, varmap& _root) {
    if (_finf.is_link()) {
        if (nullptr != _this->print_rtmsg_) _this->print_rtmsg_(_finf.path(), -1, encoder::linkfile);
        return _this->igerr_linkfile_ ? encoder::success : encoder::linkfile;
    }
    if (!_finf.is_exist()) {
        if (nullptr != _this->print_rtmsg_) _this->print_rtmsg_(_finf.path(), -1, encoder::notexist);
        return _this->igerr_notexist_ ? encoder::success : encoder::notexist;
    }
    return (_finf.is_dir() ? append_impl_dir : append_impl_file)(_finf, _name, _this, _root);
}

uint_8 encoder_base::append_impl_dir(const file_info& _finf, const std::string& _name, encoder_base* _this, varmap& _root) {
    varmap dir_root;
    for (const auto& info : _finf.get_child()) {
        uint_8 ret = append_impl(info, info.name(), _this, dir_root);
        if (encoder::success != ret) return ret;
    }

    _root.insert(_name, dir_root);
    return encoder::success;
}

uint_8 encoder_base::append_impl_file(const file_info& _finf, const std::string& _name, encoder_base* _this, varmap& _root) {
    ABORT_CHECK(_this, _finf.path(), encoder::forceabort);
    if (_root.contain(_name)) RETURN_ERROR(_this, _finf.path(), encoder::duppath);
    file fs;
    if (!fs.open(_finf, file::READ)) RETURN_ERROR(_this, _finf.path(), encoder::readfail);

    uint_64 offset{0};
    bytes data;
    varvec blocks;
    // cut the LZ4 chain at every file boundary; the decoder cuts at the same one
    _this->reset_compress();

    // comma chain, not a comparison: read the block, advance past it, stop when the read came back empty
    while (data = fs.read(offset, _this->size_), offset += data.size(), !data.empty()) {
        ABORT_CHECK(_this, _finf.path(), encoder::forceabort);
        bytes stored;
        const uint_8 ret = _this->write_block(data, _this->ostm_->total(), stored, blocks);
        if (encoder::success != ret) RETURN_ERROR(_this, _finf.path(), ret);
        if (!_this->ostm_->append(stored)) RETURN_ERROR(_this, _finf.path(), encoder::flushfail);

        if (nullptr != _this->print_rtmsg_) _this->print_rtmsg_(_finf.path(), _finf.size(), offset);
    }
    if (offset != _finf.size()) RETURN_ERROR(_this, _finf.path(), encoder::readfail);

    // closes the vec with the entry's total plain size, after the records
    blocks.push_back(offset);
    _root.insert(_name, blocks);
    return encoder::success;
}

decoder_base::decoder_base(istream* _istm)
    : istm_(_istm) {
}

decoder::pktype decoder_base::probe(const istream& _istm) {
    decoder::pktype type{};
    if (!is_accept(_istm)) return type;

    if (decoder_v2::is_accept(_istm)) {
        type.clan = true;
        type.encrypt = true;
        type.version = 2;
    } else if (decoder_v1::is_accept(_istm)) {
        const bytes_view cipher = _istm.get(ARC_COMMON_HEAD_SIZE, sizeof(uint_32));
        type.clan = true;
        type.encrypt = !cipher.empty() && ARC_AESC == cipher.to<uint_32>();
        type.version = 1;
    }
    if (0 == type.valid()) return type;

    const bytes_view cmps = _istm.get(sizeof(uint_32), sizeof(uint_32));
    type.cmps = !cmps.empty() && ARC_CMPS == cmps.to<uint_32>();
    return type;
}

decoder_base* decoder_base::create(istream* _istm, const std::string& _pswd, uint_8* _err) {
    uint_8 code = decoder::badversion;
    decoder_base* impl = nullptr;

    const decoder::pktype type = probe(*_istm);
    if (2 == type.version) impl = new decoder_v2(_istm, _pswd);
    else if (1 == type.version) impl = new decoder_v1(_istm, _pswd);

    if (nullptr == impl) {
        delete _istm;
        if (nullptr != _err) *_err = code;
        return nullptr;
    }

    code = impl->open();
    if (decoder::success != code) {
        delete impl;
        if (nullptr != _err) *_err = code;
        return nullptr;
    }
    if (nullptr != _err) *_err = decoder::success;
    return impl;
}

bool decoder_base::is_accept(const istream& _istm) {
    // the whole common header: the family magic and the compress flag
    bytes_view data = _istm.get(0, ARC_COMMON_HEAD_SIZE);
    if (data.size() != ARC_COMMON_HEAD_SIZE) return false;

    if (ARC_HEAD != data.to<uint_32>()) return false;
    const uint_32 cmps = data.to<uint_32>(sizeof(uint_32));
    return ARC_CMPS == cmps || ARC_NONE == cmps;
}

void decoder_base::enable_compress(bool _cmps) {
    if (_cmps && nullptr == cmps_) cmps_ = new compress::decoder_lz4();
    else if (!_cmps) {
        delete cmps_;
        cmps_ = nullptr;
    }
}

void decoder_base::reset_compress() const {
    if (nullptr != cmps_) cmps_->clear();
}

bool decoder_base::decompress_block(bytes& _data, uint_64 _plain_size) const {
    if (nullptr == cmps_) return true;
    _data = cmps_->exec(_data, (uint_32) _plain_size);
    return !_data.empty();
}

decoder_base::~decoder_base() {
    delete cmps_;
    delete istm_;
}

uint_8 decoder_base::open() {
    bytes_view data = istm_->get(0, ARC_COMMON_HEAD_SIZE);
    if (data.size() != ARC_COMMON_HEAD_SIZE) return decoder::badversion;

    if (ARC_CMPS == data.to<uint_32>(sizeof(uint_32))) enable_compress(true);

    // the common tail marker is the cheapest completeness check, before the version pays for its index
    const uint_64 total = istm_->total();
    if (total < ARC_COMMON_HEAD_SIZE + sizeof(uint_32)) return decoder::badversion;
    bytes_view tail = istm_->get(total - sizeof(uint_32), sizeof(uint_32));
    if (tail.size() != sizeof(uint_32) || ARC_TAIL != tail.to<uint_32>()) return decoder::badversion;

    return read_meta(archive_, fsys_);
}

bool decoder_base::is_compress() const { return nullptr != cmps_; }

const varmap& decoder_base::archive() const { return archive_; }

const varmap& decoder_base::fsystem() const { return fsys_; }

uint_8 decoder_base::get(const std::string& _root, const std::list<std::string>& _path, bytes& _odat) const {
    return fsys_.contain(_root) ? get_impl(this, fsys_.value(_root).select(_path, variant::def_val()), _odat) : decoder::notexist;
}

uint_8 decoder_base::get(const std::string& _root, const std::list<std::string>& _path, ostream& _ostm) const {
    return fsys_.contain(_root) ? get_impl(this, fsys_.value(_root).select(_path, variant::def_val()), _ostm) : decoder::notexist;
}

uint_8 decoder_base::save(const std::string& _root, const std::list<std::string>& _path, const file_info& _dir) const {
    if (_root.empty() || _root == "*") return save_impl_dir(this, fsys_, _dir, std::string());
    else if (fsys_.contain(_root)) return save_impl(this, fsys_.value(_root).select(_path, variant::def_val()), _dir, _path.empty() ? _root : _path.back());
    else return decoder::notexist;
}

void decoder_base::set_abort_flag(const bool* _abort_flag) { abort_ = _abort_flag; }

const istream* decoder_base::get_stream() const { return istm_; }

void decoder_base::set_print_rtmsg(MesgFunc _fc) { print_rtmsg_ = _fc; }

uint_8 decoder_base::get_impl(const decoder_base* _this, const variant& _ctrl, bytes& _odat) {
    ostream_buff ostm(_odat);
    return _odat.clear(), get_impl(_this, _ctrl, ostm);
}

uint_8 decoder_base::get_impl(const decoder_base* _this, const variant& _ctrl, ostream& _ostm) {
    ABORT_CHECK(_this, _ostm.name(), decoder::forceabort);
    if (_ctrl.null()) RETURN_ERROR(_this, _ostm.name(), decoder::notexist);
    if (!_ctrl.is<varvec>()) RETURN_ERROR(_this, _ostm.name(), decoder::notfile);

    varvec __blocks = _ctrl.to<varvec>();
    // a file is at least one record plus the trailing total; one slot holding 0 is an empty file
    if (__blocks.size() < 2 &&
        (__blocks.size() != 1 || !__blocks.back().is<uint_64>() || 0 != __blocks.back().to<uint_64>()))
        RETURN_ERROR(_this, _ostm.name(), decoder::badblock);
    uint_64 __total = __blocks.back().to<uint_64>();
    // the caller's stream may already hold bytes, so the totals below are measured from here
    const uint_64 ostm_ofst = _ostm.total();
    _this->reset_compress();

    uint_64 __pos = 0;
    while (__pos < __blocks.size() - 1) {
        ABORT_CHECK(_this, _ostm.name(), decoder::forceabort);
        bytes data;
        const uint_8 ret = _this->read_block(__blocks, __pos, data);
        if (decoder::success != ret) RETURN_ERROR(_this, _ostm.name(), ret);
        if (!_ostm.append(data)) RETURN_ERROR(_this, _ostm.name(), decoder::flushfail);

        if (nullptr != _this->print_rtmsg_) _this->print_rtmsg_(_ostm.name(), __total, _ostm.total() - ostm_ofst);
    }
    if (_ostm.total() - ostm_ofst != __total) RETURN_ERROR(_this, _ostm.name(), decoder::readfail);
    if (!_ostm.flush()) RETURN_ERROR(_this, _ostm.name(), decoder::flushfail);

    return decoder::success;
}

uint_8 decoder_base::save_impl(const decoder_base* _this, const variant& _fsys, const file_info& _dir, const std::string& key) {
    const std::string _key = strutil::code_conver(key, strutil::locale_format());
    if (!_dir.is_exist() || !_dir.is_dir()) RETURN_ERROR(_this, _dir.path(), decoder::needdir);
    if (_fsys.is<varmap>()) return save_impl_dir(_this, _fsys, _dir, _key);
    else if (_fsys.is<varvec>()) return save_impl_file(_this, _fsys, _dir, _key);
    else RETURN_ERROR(_this, _dir.path(), decoder::badfsys);
}

uint_8 decoder_base::save_impl_dir(const decoder_base* _this, const variant& _fsys, const file_info& _dir, const std::string& _key) {
#ifdef _WIN32
    file_info child = _key.empty() ? _dir : file_info(strutil::format("%1\\%2", _dir.path(), _key));
#else
    file_info child = _key.empty() ? _dir : file_info(strutil::format("%1/%2", _dir.path(), _key));
#endif
    if (!child.mkdir()) RETURN_ERROR(_this, _dir.path(), decoder::cdirfail);
    varmap vmap = _fsys.to<varmap>();
    for (const auto& it : vmap) {
        uint_8 ret = save_impl(_this, *it.second, child, it.first);
        if (decoder::success != ret) return ret;
    }
    return decoder::success;
}

uint_8 decoder_base::save_impl_file(const decoder_base* _this, const variant& _ctrl, const file_info& _dir, const std::string& _key) {
#ifdef _WIN32
    ostream_file ostm(file_info(strutil::format("%1\\%2", _dir.path(), _key)), false);
#else
    ostream_file ostm(file_info(strutil::format("%1/%2", _dir.path(), _key)), false);
#endif
    return get_impl(_this, _ctrl, ostm);
}
