/*****************************************************************/ /**
 * \file   afpacker.cpp
 * \brief  Public encoder/decoder shells and factories
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "afpacker.h"

#include "fpacker/afpacker_v1.h"
#include "fpacker/afpacker_v2.h"
#include "autility.h"

using namespace alx;
using namespace alx::fpacker;
using namespace alx::fpacker::detail;

encoder::encoder(detail::encoder_base* _impl)
    : impl_(_impl) {
}

encoder::~encoder() {
    // finish through the derived object: the base destructor must not dispatch pure virtuals
    impl_->finish();
    delete impl_;
}

encoder* encoder::create(bool _cmps, uint_8* _err, uint_64 _blck_size) {
    if (0 == _blck_size || _blck_size > ARC_MAX_BLCK) {
        if (nullptr != _err) *_err = encoder::badparams;
        return nullptr;
    }
    if (nullptr != _err) *_err = encoder::success;
    return new encoder(new encoder_v1(_cmps, _blck_size, std::string()));
}

encoder* encoder::create_encrypt_v1(const std::string& _pswd, bool _cmps, uint_8* _err, uint_64 _blck_size) {
    if (_pswd.empty() || 0 == _blck_size || _blck_size > ARC_MAX_BLCK) {
        if (nullptr != _err) *_err = encoder::badparams;
        return nullptr;
    }
    if (nullptr != _err) *_err = encoder::success;
    return new encoder(new encoder_v1(_cmps, _blck_size, _pswd));
}

encoder* encoder::create_encrypt_v2(const std::string& _pswd, bool _cmps, uint_8* _err, uint_64 _blck_size, uint_32 _iterations) {
    if (_pswd.empty() || _blck_size < ARC_AESG_MIN_BLCK || _blck_size > ARC_MAX_BLCK || 0 == _iterations ||
        _iterations > ARC_AESG_MAX_ITER) {
        if (nullptr != _err) *_err = encoder::badparams;
        return nullptr;
    }
    if (nullptr != _err) *_err = encoder::success;
    return new encoder(new encoder_v2(_cmps, _blck_size, _pswd, _iterations));
}

varmap& encoder::archive() { return impl_->archive(); }

bool encoder::start(ostream* _ostm) { return impl_->start(_ostm); }

bool encoder::finish() { return impl_->finish(); }

uint_8 encoder::append(const file_info& _finf, const std::string& _name) { return impl_->append(_finf, _name); }

const ostream* encoder::get_stream() const { return impl_->get_stream(); }

void encoder::set_abort_flag(const bool* _abort_flag) { impl_->set_abort_flag(_abort_flag); }

void encoder::set_print_rtmsg(MesgFunc _fc) { impl_->set_print_rtmsg(_fc); }

bool encoder::is_igerr_notexist() const { return impl_->is_igerr_notexist(); }

void encoder::set_igerr_notexist(bool _ig) { impl_->set_igerr_notexist(_ig); }

bool encoder::is_igerr_linkfile() const { return impl_->is_igerr_linkfile(); }

void encoder::set_igerr_linkfile(bool _ig) { impl_->set_igerr_linkfile(_ig); }

bool encoder::is_igerr_randfallback() const { return impl_->is_igerr_randfallback(); }

void encoder::set_igerr_randfallback(bool _ig) { impl_->set_igerr_randfallback(_ig); }

const char* encoder::error_message(uint_8 _err) {
    switch (_err) {
    case notexist: return "file_not_exist";
    case linkfile: return "unsupport_link_file";
    case duppath: return "duplicate_path";
    case readfail: return "read_failed";
    case cmpsfail: return "compression_failed";
    case flushfail: return "flush_failed";
    case forceabort: return "force_abort";
    case badparams: return "bad_parameters";
    case randfallback: return "entropy_fallback";
    case randfail: return "entropy_unavailable";
    default: return "unknown";
    }
}

decoder::decoder(detail::decoder_base* _impl)
    : impl_(_impl) {
}

decoder::~decoder() {
    delete impl_;
}

decoder* decoder::create(istream* _istm, const std::string& _pswd, uint_8* _err) {
    detail::decoder_base* impl = detail::decoder_base::create(_istm, _pswd, _err);
    return nullptr == impl ? nullptr : new decoder(impl);
}

decoder::pktype decoder::probe(const istream& _istm) { return detail::decoder_base::probe(_istm); }

const varmap& decoder::archive() const { return impl_->archive(); }

const varmap& decoder::fsystem() const { return impl_->fsystem(); }

uint_8 decoder::get(const std::string& _root, const std::list<std::string>& _path, bytes& _odat) const { return impl_->get(_root, _path, _odat); }

uint_8 decoder::get(const std::string& _root, const std::list<std::string>& _path, ostream& _ostm) const { return impl_->get(_root, _path, _ostm); }

uint_8 decoder::save(const std::string& _root, const std::list<std::string>& _path, const file_info& _dir) const { return impl_->save(_root, _path, _dir); }

void decoder::set_abort_flag(const bool* _abort_flag) { impl_->set_abort_flag(_abort_flag); }

const istream* decoder::get_stream() const { return impl_->get_stream(); }

bool decoder::is_encrypt() const { return impl_->is_encrypt(); }

bool decoder::is_compress() const { return impl_->is_compress(); }

void decoder::set_print_rtmsg(MesgFunc _fc) { impl_->set_print_rtmsg(_fc); }

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
    case badversion: return "bad_version";
    case badparams: return "bad_parameters";
    case badauth: return "authentication_failed";
    default: return "unknown";
    }
}
