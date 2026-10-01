/*****************************************************************/ /**
 * \file   aregex_pcre2.cpp
 * \brief  PCRE2-backed regular expression engine
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "aregex_pcre2.h"

// Must precede <pcre2.h>, and 8 is the code-unit width of the byte-oriented API used here.
#define PCRE2_CODE_UNIT_WIDTH 8
#define PCRE2_STATIC
#include <pcre2.h>

#include <cstring>

static_assert(alx::regex_pcre2::icase == PCRE2_CASELESS, "regex_pcre2::icase drifted from PCRE2_CASELESS");
static_assert(alx::regex_pcre2::dotall == PCRE2_DOTALL, "regex_pcre2::dotall drifted from PCRE2_DOTALL");
static_assert(alx::regex_pcre2::multiline == PCRE2_MULTILINE, "regex_pcre2::multiline drifted from PCRE2_MULTILINE");
static_assert(alx::regex_pcre2::nosubs == PCRE2_NO_AUTO_CAPTURE, "regex_pcre2::nosubs drifted from PCRE2_NO_AUTO_CAPTURE");

namespace alx {
    namespace {

        struct callout_binding {
            bool (*fn)(void*);
            void* ud;
        };

        // Only a negative return abandons the match: a positive one just fails this path, i.e. a plain no_match.
        int callout_bridge(pcre2_callout_block*, void* _ud) {
            const callout_binding* bind = static_cast<const callout_binding*>(_ud);
            if (nullptr == bind || nullptr == bind->fn) return 0;
            return bind->fn(bind->ud) ? 0 : PCRE2_ERROR_CALLOUT;
        }

        uint_32 clamp_limit(uint_64 _value) {
            return (_value > 0xFFFFFFFFull) ? 0xFFFFFFFFu : (uint_32) _value;
        }

        void push_groups(std::vector<std::string>& _out, const char* _ptr, const PCRE2_SIZE* _ov, uint_32 _pairs) {
            for (uint_32 i = 0; i < _pairs; ++i) {
                // PCRE2_UNSET is not an offset: an unset group comes back empty, never as a pointer.
                if (PCRE2_UNSET == _ov[2 * i]) _out.emplace_back();
                else _out.emplace_back(_ptr + _ov[2 * i], _ov[2 * i + 1] - _ov[2 * i]);
            }
        }

        template <typename OnHit>
        int scan_all(void* _code, void* _mctx, void* _mdata, const char* _ptr, uint_64 _size, OnHit _on_hit) {
            PCRE2_SIZE offset = 0;
            uint_32 opt = 0;
            while (true) {
                int rc = pcre2_match((pcre2_code*) _code, (PCRE2_SPTR) _ptr, _size, offset, opt,
                                     (pcre2_match_data*) _mdata, (pcre2_match_context*) _mctx);
                if (PCRE2_ERROR_NOMATCH == rc) {
                    // opt == 0 means the plain search ran out; otherwise the anchored retry failed here, so step a byte on.
                    if (0 == opt || offset >= _size) return rc;
                    ++offset;
                    opt = 0;
                    continue;
                }
                if (rc < 0) return rc;

                const PCRE2_SIZE* ov = pcre2_get_ovector_pointer((pcre2_match_data*) _mdata);
                _on_hit(ov);
                if (ov[0] == ov[1]) {

                    // An empty match is retried at its own start under NOTEMPTY_ATSTART|ANCHORED; ov[0] and
                    // not the search offset, because \K in a lookaround can report a start behind it.
                    offset = ov[0];
                    // Zero width at the very end: nothing left to scan, and the rc is a success, not a no_match.
                    if (offset >= _size) return 1;
                    opt = PCRE2_NOTEMPTY_ATSTART | PCRE2_ANCHORED;
                } else {
                    offset = ov[1];
                    opt = 0;
                }
            }
        }
    }
}

alx::regex_pcre2::~regex_pcre2() {
    release();
}

alx::regex_pcre2::regex_pcre2(const std::string& _reg_str, uint_32 _opt) noexcept
    : pattern(_reg_str) {
    int err = 0;
    PCRE2_SIZE off = 0;

    // AUTO_CALLOUT is always added: without callout points in the compiled pattern set_callout() would never fire.
    code_ = pcre2_compile((PCRE2_SPTR) pattern.c_str(), pattern.length(), _opt | PCRE2_AUTO_CALLOUT, &err, &off, nullptr);
    if (nullptr == code_) {
        last_err_ = err;
        status_ = match_status::error;
        return;
    }

    mctx_ = pcre2_match_context_create(nullptr);
    mdata_ = pcre2_match_data_create_from_pattern((pcre2_code*) code_, nullptr);
    if (nullptr == mctx_ || nullptr == mdata_) {
        release();
        status_ = match_status::error;
    }
}

alx::regex_pcre2::regex_pcre2(regex_pcre2&& _regx) noexcept
    : code_(_regx.code_), mctx_(_regx.mctx_), mdata_(_regx.mdata_), callout_(_regx.callout_),
      sub_opt_(_regx.sub_opt_), last_err_(_regx.last_err_), status_(_regx.status_),
      pattern(std::move(_regx.pattern)) {
    _regx.code_ = nullptr;
    _regx.mctx_ = nullptr;
    _regx.mdata_ = nullptr;
    _regx.callout_ = nullptr;
}

alx::regex_pcre2& alx::regex_pcre2::operator=(regex_pcre2&& _regx) noexcept {
    if (this == &_regx) return *this;
    release();
    code_ = _regx.code_;
    mctx_ = _regx.mctx_;
    mdata_ = _regx.mdata_;
    callout_ = _regx.callout_;
    sub_opt_ = _regx.sub_opt_;
    last_err_ = _regx.last_err_;
    status_ = _regx.status_;
    pattern = std::move(_regx.pattern);
    _regx.code_ = nullptr;
    _regx.mctx_ = nullptr;
    _regx.mdata_ = nullptr;
    _regx.callout_ = nullptr;
    return *this;
}

bool alx::regex_pcre2::is_compliant(const char* _ptr, uint_64 _size) const {
    if (uint_64_npos == _size) _size = strlen(_ptr);
    if (nullptr == code_) return false;

    int rc = pcre2_match((pcre2_code*) code_, (PCRE2_SPTR) _ptr, _size, 0, PCRE2_ANCHORED | PCRE2_ENDANCHORED,
                         (pcre2_match_data*) mdata_, (pcre2_match_context*) mctx_);
    note_result(rc);
    return rc >= 0;
}

std::string alx::regex_pcre2::find(const char* _ptr, uint_64 _size) const {
    if (uint_64_npos == _size) _size = strlen(_ptr);
    if (nullptr == code_) return std::string();

    int rc = pcre2_match((pcre2_code*) code_, (PCRE2_SPTR) _ptr, _size, 0, 0, (pcre2_match_data*) mdata_,
                         (pcre2_match_context*) mctx_);
    note_result(rc);
    if (rc < 0) return std::string();

    const PCRE2_SIZE* ov = pcre2_get_ovector_pointer((pcre2_match_data*) mdata_);
    return std::string(_ptr + ov[0], ov[1] - ov[0]);
}

std::vector<std::string> alx::regex_pcre2::find_all(const char* _ptr, uint_64 _size) const {
    if (uint_64_npos == _size) _size = strlen(_ptr);
    std::vector<std::string> result;

    for (const auto& span : find_all_spans(_ptr, _size)) result.emplace_back(_ptr + span.first, span.second);
    return result;
}

std::vector<std::pair<alx::uint_64, alx::uint_64>> alx::regex_pcre2::find_all_spans(const char* _ptr, uint_64 _size) const {
    if (uint_64_npos == _size) _size = strlen(_ptr);
    std::vector<std::pair<alx::uint_64, alx::uint_64>> result;
    if (nullptr == code_) return result;

    note_result(scan_all(code_, mctx_, mdata_, _ptr, _size, [&result](const PCRE2_SIZE* _ov) {
        result.emplace_back((uint_64) _ov[0], (uint_64) (_ov[1] - _ov[0]));
    }));
    return result;
}

std::vector<std::string> alx::regex_pcre2::match_group(const char* _ptr, uint_64 _size) const {
    if (uint_64_npos == _size) _size = strlen(_ptr);
    std::vector<std::string> result;
    if (nullptr == code_) return result;

    int rc = pcre2_match((pcre2_code*) code_, (PCRE2_SPTR) _ptr, _size, 0, 0, (pcre2_match_data*) mdata_,
                         (pcre2_match_context*) mctx_);
    note_result(rc);
    if (rc < 0) return result;

    push_groups(result, _ptr, pcre2_get_ovector_pointer((pcre2_match_data*) mdata_),
                pcre2_get_ovector_count((pcre2_match_data*) mdata_));
    return result;
}

std::vector<std::string> alx::regex_pcre2::match_group_all(const char* _ptr, uint_64 _size) const {
    if (uint_64_npos == _size) _size = strlen(_ptr);
    std::vector<std::string> result;
    if (nullptr == code_) return result;

    note_result(scan_all(code_, mctx_, mdata_, _ptr, _size, [this, _ptr, &result](const PCRE2_SIZE* _ov) {
        push_groups(result, _ptr, _ov, pcre2_get_ovector_count((pcre2_match_data*) mdata_));
    }));
    return result;
}

std::string alx::regex_pcre2::replace(const std::string& _str, const std::string& _repl) const {
    return substitute(_str, _repl, false);
}

std::string alx::regex_pcre2::replace_all(const std::string& _str, const std::string& _repl) const {
    return substitute(_str, _repl, true);
}

void alx::regex_pcre2::set_step_limit(uint_64 _steps) {
    if (nullptr == mctx_) return;

    uint_32 want = clamp_limit(_steps);
    // 0 would abort every match, so it asks for the engine default instead; a refused query leaves the limit in place.
    if (0 == _steps && 0 != pcre2_config(PCRE2_CONFIG_MATCHLIMIT, &want)) return;
    pcre2_set_match_limit((pcre2_match_context*) mctx_, want);
}

void alx::regex_pcre2::set_depth_limit(uint_64 _depth) {
    if (nullptr == mctx_) return;

    uint_32 want = clamp_limit(_depth);
    if (0 == _depth && 0 != pcre2_config(PCRE2_CONFIG_DEPTHLIMIT, &want)) return;
    pcre2_set_depth_limit((pcre2_match_context*) mctx_, want);
}

void alx::regex_pcre2::set_callout(bool (*_fn)(void*), void* _ud) {
    if (nullptr == mctx_) return;

    // Allocate before the old binding is deleted: a throw here must leave the context holding a live hook or none.
    callout_binding* bind = (nullptr != _fn) ? new callout_binding{_fn, _ud} : nullptr;
    delete (callout_binding*) callout_;
    callout_ = bind;
    pcre2_set_callout((pcre2_match_context*) mctx_, (nullptr != bind) ? callout_bridge : nullptr, bind);
}

void alx::regex_pcre2::set_substitute_options(uint_32 _opt) {
    sub_opt_ = _opt;
}

void alx::regex_pcre2::release() noexcept {
    pcre2_match_data_free((pcre2_match_data*) mdata_);
    pcre2_match_context_free((pcre2_match_context*) mctx_);
    pcre2_code_free((pcre2_code*) code_);
    delete (callout_binding*) callout_;
    mdata_ = nullptr;
    mctx_ = nullptr;
    code_ = nullptr;
    callout_ = nullptr;
}

void alx::regex_pcre2::note_result(int _rc) const {
    last_err_ = (_rc < 0) ? _rc : 0;
    if (_rc >= 0) {
        status_ = match_status::ok;
        return;
    }

    switch (_rc) {
    case PCRE2_ERROR_NOMATCH: status_ = match_status::no_match; break;
    case PCRE2_ERROR_MATCHLIMIT:
    case PCRE2_ERROR_DEPTHLIMIT: status_ = match_status::limit; break;
    case PCRE2_ERROR_CALLOUT: status_ = match_status::interrupted; break;
    default: status_ = match_status::error; break;
    }
}

std::string alx::regex_pcre2::substitute(const std::string& _str, const std::string& _repl, bool _global) const {
    if (nullptr == code_) return _str;

    uint_32 opt = sub_opt_ | PCRE2_SUBSTITUTE_OVERFLOW_LENGTH;
    if (_global) opt |= PCRE2_SUBSTITUTE_GLOBAL;

    std::string out(_str.length() + 64, '\0');
    for (int attempt = 0; attempt < 4; ++attempt) {
        PCRE2_SIZE len = out.length();
        int rc = pcre2_substitute((pcre2_code*) code_, (PCRE2_SPTR) _str.c_str(), _str.length(), 0, opt,
                                  (pcre2_match_data*) mdata_, (pcre2_match_context*) mctx_,
                                  (PCRE2_SPTR) _repl.c_str(), _repl.length(), (PCRE2_UCHAR*) out.data(), &len);
        // OVERFLOW_LENGTH makes len come back as the size the result needs, so the retry sizes the buffer to it.
        if (PCRE2_ERROR_NOMEMORY == rc) {
            out.resize(len);
            continue;
        }

        note_result(rc);
        if (rc < 0) return _str;
        out.resize(len);
        return out;
    }
    return _str;
}
