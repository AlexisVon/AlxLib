/*****************************************************************/ /**
 * \file   astream_ex.cpp
 * \brief  File-backed stream implementations
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "astream_ex.h"

using namespace alx;

ostream_file::ostream_file(const file_info& _file, bool _append, uint_64 _buf_size, std::function<bytes(const void*, uint_64)> _pre_exec)
    : ostream(), buf_size_(_buf_size), open_type_(_append ? file::APED : file::WRIT), dat_ofst_(0), dat_buff_(new uint_8[_buf_size]), dat_total_(0) {
    file_.open(_file, open_type_);
    name_ = _file.path();
    if (nullptr == _pre_exec)
        fun_flush_ = [this](const void* _d, uint_64 _s) -> bool {
            return file_.write(_d, _s) && file_.flush();
        };
    else fun_flush_ = [this, _pre_exec](const void* _d, uint_64 _s) -> bool {
        return file_.write(_pre_exec(_d, _s)) && file_.flush();
    };
}

ostream_file::~ostream_file() {
    flush();
    file_.close();
    delete[] dat_buff_;
}

bool ostream_file::append(const void* _ptr, uint_64 _size) {
    bool ret = true;
    const uint_64 new_size = dat_ofst_ + _size;
    if (new_size > buf_size_) {
        // a failed flush short-circuits: the block is dropped, not buffered for a retry
        ret = flush() && fun_flush_(_ptr, _size);
    } else {
        memcpy(dat_buff_ + dat_ofst_, _ptr, _size);
        dat_ofst_ = new_size;
    }
    if (ret) dat_total_ += _size;
    return ret;
}

bool ostream_file::flush() {
    bool ret{true};
    // a failed write leaves dat_ofst_ untouched: the same bytes go out on the next flush or in the destructor
    if (0 != dat_ofst_) {
        ret = fun_flush_(dat_buff_, dat_ofst_);
        if (ret) dat_ofst_ = 0;
    }
    return ret;
}

istream_file::istream_file(const file_info& _file, std::function<void(bytes&)> _pre_exec)
    : istream(), pre_exec_(_pre_exec) {
    file_.open(_file, file::READ);
    name_ = _file.path();
}

istream_file::~istream_file() {
    file_.close();
}

uint_64 istream_file::total() const {
    return file_.info().size();
}

bytes_view istream_file::get(uint_64 _ofst, uint_64 _size) const {
    bytes result = file_.read(_ofst, _size);
    if (nullptr != pre_exec_) pre_exec_(result);
    return result;
}
