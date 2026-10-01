/*****************************************************************/ /**
 * \file   aregex_ex.cpp
 * \brief  Regular expression extensions
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "aregex_ex.h"

#include <cstring>

alx::regex_ex::regex_ex(const std::string& _reg_str, std::regex::flag_type _opt) noexcept
    : pattern(_reg_str) {
    try {
        regx = new std::regex(_reg_str, _opt);
    } catch (...) {
        regx = nullptr;
    }
}

alx::regex_ex::regex_ex(const regex_ex& _regx) noexcept
    : pattern(_regx.pattern) {
    if (nullptr != _regx.regx) {
        try {
            regx = new std::regex(*_regx.regx);
        } catch (...) {
            regx = nullptr;
        }
    }
}

alx::regex_ex& alx::regex_ex::operator=(const regex_ex& _regx) noexcept {
    // self-assignment: the delete below would free the source's compiled pattern with ours
    if (this == &_regx) return *this;
    pattern = _regx.pattern;
    // nulled here and by the catch below: no path out of this function leaves regx dangling
    delete regx;
    regx = nullptr;
    if (nullptr != _regx.regx) {
        try {
            regx = new std::regex(*_regx.regx);
        } catch (...) {
            regx = nullptr;
        }
    }
    return *this;
}

alx::regex_ex::regex_ex(regex_ex&& _regx) noexcept {
    std::swap(regx, _regx.regx);
    pattern = std::move(_regx.pattern);
}

alx::regex_ex& alx::regex_ex::operator=(regex_ex&& _regx) noexcept {
    if (this == &_regx) return *this;
    std::swap(regx, _regx.regx);
    pattern = std::move(_regx.pattern);
    return *this;
}

bool alx::regex_ex::is_compliant(const char* _ptr, uint_64 _size) const {
    if (uint_64_npos == _size) _size = strlen(_ptr);
    return std::regex_match(_ptr, _ptr + _size, *regx);
}

std::string alx::regex_ex::find(const char* _ptr, uint_64 _size) const {
    if (uint_64_npos == _size) _size = strlen(_ptr);

    std::cmatch matches;
    if (std::regex_search(_ptr, _ptr + _size, matches, *regx)) {
        return std::string(matches[0].first, matches[0].second);
    }

    return std::string();
}

std::vector<std::string> alx::regex_ex::find_all(const char* _ptr, uint_64 _size) const {
    if (uint_64_npos == _size) _size = strlen(_ptr);
    std::vector<std::string> result;
    std::cregex_iterator iter(_ptr, _ptr + _size, *regx);
    std::cregex_iterator end;

    for (; iter != end; ++iter) {
        std::cmatch match = *iter;
        result.emplace_back(match[0].first, match[0].second);
    }

    return result;
}

std::vector<std::string> alx::regex_ex::match_group(const char* _ptr, uint_64 _size) const {
    if (uint_64_npos == _size) _size = strlen(_ptr);
    std::vector<std::string> result;
    std::cmatch matches;

    if (std::regex_search(_ptr, _ptr + _size, matches, *regx)) {
        for (size_t i = 0; i < matches.size(); ++i) {
            result.emplace_back(matches[i].first, matches[i].second);
        }
    }

    return result;
}

std::vector<std::string> alx::regex_ex::match_group_all(const char* _ptr, uint_64 _size) const {
    if (uint_64_npos == _size) _size = strlen(_ptr);

    std::vector<std::string> result;
    std::cregex_iterator iter(_ptr, _ptr + _size, *regx);
    std::cregex_iterator end;

    for (; iter != end; ++iter) {
        std::cmatch match = *iter;
        for (size_t i = 0; i < match.size(); ++i) {
            result.emplace_back(match[i].first, match[i].second);
        }
    }

    return result;
}

std::string alx::regex_ex::replace(const std::string& _str, const std::string& _repl) const {
    return std::regex_replace(_str, *regx, _repl, std::regex_constants::format_first_only);
}

std::string alx::regex_ex::replace_all(const std::string& _str, const std::string& _repl) const {
    return std::regex_replace(_str, *regx, _repl, std::regex_constants::format_default);
}