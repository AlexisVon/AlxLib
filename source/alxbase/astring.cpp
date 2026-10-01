/*****************************************************************/ /**
 * \file   astring.cpp
 * \brief  String processing utilities (multi-byte/wide character support)
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "astring.h"
#include "aalgo.h"
#include "aregex_ex.h"
#include <cctype>
#include <vector>

#ifdef _WIN32
#    include <Windows.h>
#else
#    include <iconv.h>
#endif

using namespace alx;

#ifdef _WIN32
std::wstring alx::strutil::to_wstring(const char* _str, uint_64 _len) {
    if (nullptr == _str) return std::wstring();
    if (_len == uint_64_npos) _len = strlen(_str);
    std::wstring result(MultiByteToWideChar(CP_ACP, 0, _str, (int) _len, NULL, 0), L'\0');
    MultiByteToWideChar(CP_ACP, 0, _str, (int) _len, &result[0], (int) result.length());
    return result;
}

std::string alx::strutil::from_wstring(const wchar_t* _str, uint_64 _len) {
    if (nullptr == _str) return std::string();
    if (_len == uint_64_npos) _len = lstrlenW(_str);
    std::string result(WideCharToMultiByte(CP_ACP, 0, _str, (int) _len, NULL, 0, NULL, NULL), '\0');
    WideCharToMultiByte(CP_ACP, 0, _str, (int) _len, &result[0], (int) result.length(), NULL, NULL);
    return result;
}

strutil::CODE_FORMAT alx::strutil::locale_format() {
    UINT code_page = GetACP();
    if (code_page == 65001) return UTF8;
    else if (code_page == 1200) return UTF16_LE;
    else if (code_page == 1201) return UTF16_BE;
    else return GBK;
}
#else
std::wstring alx::strutil::to_wstring(const char* _str, uint_64 _len) {
    if (nullptr == _str) return std::wstring();
    if (uint_64_npos == _len) _len = strlen(_str);

    setlocale(LC_CTYPE, "");
    const char* ptr = _str;
    size_t wc_count = mbsnrtowcs(nullptr, &ptr, _len, 0, nullptr);
    if (0 == wc_count || (size_t) -1 == wc_count) return std::wstring();

    std::wstring result(wc_count, L'\0');
    ptr = _str;
    mbsnrtowcs(&result[0], &ptr, _len, wc_count, nullptr);
    return result;
}

std::string alx::strutil::from_wstring(const wchar_t* _str, uint_64 _len) {
    if (nullptr == _str) return std::string();
    if (uint_64_npos == _len) _len = wcslen(_str);

    setlocale(LC_CTYPE, "");
    const wchar_t* ptr = _str;
    size_t mb_count = wcsnrtombs(nullptr, &ptr, _len, 0, nullptr);
    if (mb_count <= 0) return std::string();

    std::string result(mb_count, '\0');
    ptr = _str;
    wcsnrtombs(&result[0], &ptr, _len, mb_count, nullptr);

    return result;
}

strutil::CODE_FORMAT alx::strutil::locale_format() {
    strutil::CODE_FORMAT result = []() -> strutil::CODE_FORMAT {
        setlocale(LC_ALL, "");
        const char* locale = setlocale(LC_CTYPE, nullptr);
        if (locale != nullptr) {
            std::string locale_str(locale);
            for (char& ch : locale_str)
                if (ch >= 'A' && ch <= 'Z') ch += 32;
            regex_ex utf8_reg(".*utf.?8.*");
            regex_ex utf16le_reg(".*utf.?16.*(le)?.*");
            regex_ex utf16be_reg(".*utf.?16.*(be).*");
            regex_ex gbk_reg(".*(gbk|gb2312|chinese|zh[_-]cn).*");
            if (utf8_reg.is_compliant(locale_str)) return UTF8;
            if (utf16le_reg.is_compliant(locale_str)) return UTF16_LE;
            // unreachable: the LE pattern above matches every UTF-16 name, its (le) being optional
            if (utf16be_reg.is_compliant(locale_str)) return UTF16_BE;
            if (gbk_reg.is_compliant(locale_str)) return GBK;
            else return UTF8;
        }
        return UTF8;
    }();
    return result;
}

#endif

strutil::CODE_FORMAT alx::strutil::detect_format(const char* _str, uint_64 _len, bool _bom_only) {
    if (_len < 2) return GBK;
    if (_len >= 3 &&
        static_cast<unsigned char>(_str[0]) == 0xEF &&
        static_cast<unsigned char>(_str[1]) == 0xBB &&
        static_cast<unsigned char>(_str[2]) == 0xBF) return UTF8_BOM;
    if (_len >= 2) {
        if (static_cast<unsigned char>(_str[0]) == 0xFF &&
            static_cast<unsigned char>(_str[1]) == 0xFE) return UTF16_LE_BOM;
        if (static_cast<unsigned char>(_str[0]) == 0xFE &&
            static_cast<unsigned char>(_str[1]) == 0xFF) return UTF16_BE_BOM;
    }
    if (_bom_only) return GBK;

    // the window bounds the scan only; a character straddling it still counts while its bytes are in the buffer
    const size_t check_len = alx::min_value<size_t>(1024, _len);
    bool is_valid_utf8{true};
    bool has_extended_chars{false};
    for (size_t i = 0; i < check_len;) {
        unsigned char byte = static_cast<unsigned char>(_str[i]);
        if ((byte & 0x80) == 0) i++;
        else if ((byte & 0xE0) == 0xC0) {
            if (i + 1 >= _len ||
                (static_cast<unsigned char>(_str[i + 1]) & 0xC0) != 0x80) {
                is_valid_utf8 = false;
                break;
            }
            has_extended_chars = true;
            i += 2;
        } else if ((byte & 0xF0) == 0xE0) {
            if (i + 2 >= _len ||
                (static_cast<unsigned char>(_str[i + 1]) & 0xC0) != 0x80 ||
                (static_cast<unsigned char>(_str[i + 2]) & 0xC0) != 0x80) {
                is_valid_utf8 = false;
                break;
            }
            has_extended_chars = true;
            i += 3;
        } else if ((byte & 0xF8) == 0xF0) {
            if (i + 3 >= _len ||
                (static_cast<unsigned char>(_str[i + 1]) & 0xC0) != 0x80 ||
                (static_cast<unsigned char>(_str[i + 2]) & 0xC0) != 0x80 ||
                (static_cast<unsigned char>(_str[i + 3]) & 0xC0) != 0x80) {
                is_valid_utf8 = false;
                break;
            }
            has_extended_chars = true;
            i += 4;
        } else {
            is_valid_utf8 = false;
            break;
        }
    }
    if (is_valid_utf8 && has_extended_chars) return UTF8;
    if ((_len & 0X01U) == 0) {
        int high_nulls = 0, low_nulls = 0;
        for (size_t i = 0; i < check_len; i += 2) {
            if (_str[i] == '\0') high_nulls++;
            if (_str[i + 1] == '\0') low_nulls++;
        }
        if (high_nulls + low_nulls > static_cast<int>(check_len >> 2))
            return high_nulls > low_nulls ? UTF16_BE : UTF16_LE;
    }

    return GBK;
}

template <typename RET_TYPE>
RET_TYPE code_conver_impl(const char* _str, const uint_64 _len, strutil::CODE_FORMAT _dst_format, strutil::CODE_FORMAT _src_format) {
    if (_len == 0) return RET_TYPE();
    if (_dst_format == strutil::AUTO) return RET_TYPE(_str, _len);
    _src_format = strutil::AUTO == _src_format ? strutil::detect_format(_str, _len) : _src_format;
    if (_src_format == _dst_format) return RET_TYPE(_str, _len);

    size_t src_offset{
        _src_format & strutil::PROPERTY_BOM ? _src_format & strutil::ENCODE_UTF8 ? 3LLU : _src_format & strutil::ENCODE_UTF16 ? 2LLU
                                                                                                                              : 0LLU
                                            : 0LLU};
    size_t dst_offset{
        _dst_format & strutil::PROPERTY_BOM ? _dst_format & strutil::ENCODE_UTF8 ? 3LLU : _dst_format & strutil::ENCODE_UTF16 ? 2LLU
                                                                                                                              : 0LLU
                                            : 0LLU};

#ifdef _WIN32
    const UINT src_page{
        _src_format & strutil::ENCODE_GBK ? (UINT) CP_ACP : _src_format & strutil::ENCODE_UTF8 ? (UINT) CP_UTF8
                                                        : _src_format & strutil::ENCODE_UTF16  ? _src_format & strutil::PROPERTY_LE ? 1200 : _src_format & strutil::PROPERTY_BE ? 1201
                                                                                                                                                                                : 0XFFFFU
                                                                                               : 0XFFFFU};
    const UINT dst_page{
        _dst_format & strutil::ENCODE_GBK ? (UINT) CP_ACP : _dst_format & strutil::ENCODE_UTF8 ? (UINT) CP_UTF8
                                                        : _dst_format & strutil::ENCODE_UTF16  ? _dst_format & strutil::PROPERTY_LE ? 1200 : _dst_format & strutil::PROPERTY_BE ? 1201
                                                                                                                                                                                : 0XFFFFU
                                                                                               : 0XFFFFU};

    if (src_offset >= _len || src_page == 0XFFFFU || dst_page == 0XFFFFU) return RET_TYPE();

    if (_src_format & strutil::PROPERTY_WIDE && _dst_format & strutil::PROPERTY_WIDE) {
        if ((_len - src_offset) & 0X01U) return RET_TYPE();
        const size_t byte_len = _len - src_offset;
        const size_t count = byte_len >> 1;
        RET_TYPE result(dst_offset + byte_len, '\0');
        const uint_16* src = reinterpret_cast<const uint_16*>(_str + src_offset);
        uint_16* dst = reinterpret_cast<uint_16*>(&result[dst_offset]);
        if ((_src_format & strutil::PROPERTY_BE) == (_dst_format & strutil::PROPERTY_BE))
            memcpy(dst, src, byte_len);
        else for (size_t i = 0; i < count; i++) dst[i] = (src[i] << 8) | (src[i] >> 8);
        if (dst_offset != 0)
            if (_dst_format & strutil::PROPERTY_BE)
                result[0] = static_cast<char>(0XFEU), result[1] = static_cast<char>(0XFFU);
            else  result[0] = static_cast<char>(0XFFU), result[1] = static_cast<char>(0XFEU);
        return result;
    }

    std::wstring wide_str;
    if (_src_format & strutil::PROPERTY_MULTI) {
        int wide_char_count = MultiByteToWideChar(src_page, 0, _str + src_offset, (int)(_len - src_offset), NULL, 0);
        if (wide_char_count <= 0) return RET_TYPE();
        wide_str.resize(wide_char_count);
        MultiByteToWideChar(src_page, 0, _str + src_offset, (int)(_len - src_offset), &wide_str[0], wide_char_count);
    }
    else if (_src_format & strutil::PROPERTY_WIDE) {
        // a Windows wchar_t is one 16-bit unit, so the UTF-16 bytes copy in as they are
        if ((_len - src_offset) & 0X01U) return RET_TYPE();
        wide_str.resize((_len - src_offset) >> 1);
        memcpy(&wide_str[0], _str + src_offset, _len - src_offset);
        if (_src_format & strutil::PROPERTY_BE)
            for (wchar_t& ch : wide_str) ch = (ch << 8) | (ch >> 8);
    }
    else return RET_TYPE();

    if (_dst_format & strutil::PROPERTY_WIDE) {
        size_t byte_len = wide_str.length() * sizeof(wchar_t);
        RET_TYPE result(dst_offset + byte_len, '\0');
        wchar_t* dst = reinterpret_cast<wchar_t*>(&result[dst_offset]);
        memcpy(dst, wide_str.data(), byte_len);
        if (_dst_format & strutil::PROPERTY_BE)
            for (size_t i = 0; i < wide_str.length(); i++)
                dst[i] = (dst[i] << 8) | (dst[i] >> 8);
        if (dst_offset != 0)
            if (_dst_format & strutil::PROPERTY_BE)
                result[0] = static_cast<char>(0XFEU), result[1] = static_cast<char>(0XFFU);
            else  result[0] = static_cast<char>(0XFFU), result[1] = static_cast<char>(0XFEU);
        return result;
    }
    else if(_dst_format & strutil::PROPERTY_MULTI) {
        int mb_char_count = WideCharToMultiByte(dst_page, 0, wide_str.data(), (int)wide_str.length(), NULL, 0, NULL, NULL);
        if (mb_char_count <= 0) return RET_TYPE();
        RET_TYPE result(dst_offset + mb_char_count, '\0');
        WideCharToMultiByte(dst_page, 0, wide_str.data(), (int)wide_str.length(), (char*)result.data() + dst_offset, mb_char_count, NULL, NULL);

        if (dst_offset != 0)
            result[0] = static_cast<char>(0XEFU), result[1] = static_cast<char>(0XBBU), result[2] = static_cast<char>(0XBFU);
        return result;
    }
    else return RET_TYPE();
#else
    // explicit-endian iconv names carry no BOM, so the offsets above skip it and write it back
    const char* src_encoding{
        _src_format & strutil::ENCODE_GBK ? "GBK" : _src_format & strutil::ENCODE_UTF8 ? "UTF-8"
                                                : _src_format & strutil::ENCODE_UTF16  ? _src_format & strutil::PROPERTY_LE ? "UTF-16LE" : _src_format & strutil::PROPERTY_BE ? "UTF-16BE"
                                                                                                                                                                              : nullptr
                                                                                       : nullptr};
    const char* dst_encoding{
        _dst_format & strutil::ENCODE_GBK ? "GBK" : _dst_format & strutil::ENCODE_UTF8 ? "UTF-8"
                                                : _dst_format & strutil::ENCODE_UTF16  ? _dst_format & strutil::PROPERTY_LE ? "UTF-16LE" : _dst_format & strutil::PROPERTY_BE ? "UTF-16BE"
                                                                                                                                                                              : nullptr
                                                                                       : nullptr};
    if (src_offset >= _len || nullptr == src_encoding || nullptr == dst_encoding) return RET_TYPE();
    iconv_t cd = iconv_open(dst_encoding, src_encoding);
    if (cd == (iconv_t) -1) return RET_TYPE();
    size_t in_size = _len - src_offset;
    const char* in_buf = _str + src_offset;

    size_t out_size = in_size << 2;
    std::vector<char> out_buffer(out_size);
    char* out_buf = out_buffer.data();
    size_t out_bytes_left = out_size;

    char* in_ptr = const_cast<char*>(in_buf);
    char* out_ptr = out_buf;

    size_t original_out_bytes = out_bytes_left;
    if (iconv(cd, &in_ptr, &in_size, &out_ptr, &out_bytes_left) == (size_t) -1) {
        iconv_close(cd);
        return RET_TYPE();
    }
    iconv_close(cd);

    size_t converted_bytes = original_out_bytes - out_bytes_left;

    RET_TYPE result(converted_bytes + dst_offset, '\0');
    memcpy(&result[dst_offset], out_buf, converted_bytes);
    if (dst_offset != 0) {
        if (_dst_format & strutil::ENCODE_UTF8) result[0] = static_cast<char>(0XEFU), result[1] = static_cast<char>(0XBBU), result[2] = static_cast<char>(0XBFU);
        if (_dst_format & strutil::ENCODE_UTF16 && _dst_format & strutil::PROPERTY_LE) result[0] = static_cast<char>(0XFFU), result[1] = static_cast<char>(0XFEU);
        if (_dst_format & strutil::ENCODE_UTF16 && _dst_format & strutil::PROPERTY_BE) result[0] = static_cast<char>(0XFEU), result[1] = static_cast<char>(0XFFU);
    }
    return result;
#endif
}

std::string alx::strutil::code_conver(const char* _ptr, size_t _len, CODE_FORMAT _dst_format, CODE_FORMAT _src_format) {
    return code_conver_impl<std::string>(_ptr, _len, _dst_format, _src_format);
}

std::string alx::strutil::format(const std::string& _format, const std::vector<std::string>& _args) {
    size_t result_size{_format.size()}, pos{0};
    std::string result;
    while (pos < _format.size()) {
        char ch = _format[pos];
        if (ch == '%') {
            if (pos + 1 < _format.size() && _format[pos + 1] == '%') {
                result_size -= 1;
                pos += 2;
            } else if (pos + 1 < _format.size() && isdigit(_format[pos + 1])) {
                size_t end = pos + 1;
                while (end < _format.size() && isdigit(_format[end])) ++end;
                int index = std::atoi(&_format[pos + 1]);
                if (index > 0 && index <= static_cast<int>(_args.size())) {
                    result_size += _args[static_cast<size_t>(index - 1)].size() - end + pos;
                }
                pos = end;
            } else ++pos;
        } else ++pos;
    }
    result.reserve(result_size);
    pos = 0;
    while (pos < _format.size()) {
        char ch = _format[pos];
        if (ch == '%') {
            if (pos + 1 < _format.size() && _format[pos + 1] == '%') {
                result += '%';
                pos += 2;
            } else if (pos + 1 < _format.size() && isdigit(_format[pos + 1])) {
                size_t end = pos + 1;
                while (end < _format.size() && isdigit(_format[end])) ++end;
                int index = std::atoi(&_format[pos + 1]);
                if (index > 0 && index <= static_cast<int>(_args.size())) result += _args[static_cast<size_t>(index - 1)];
                else result.append(&_format[pos], end - pos);
                pos = end;
            } else ++pos;
        } else {
            result += ch;
            ++pos;
        }
    }
    return result;
}

std::string alx::strutil::left(const char* _ptr, size_t _len, size_t _index, size_t _length) {
    if (_index >= _len) return std::string();
    const size_t max_len = _index + 1;
    const size_t len = _length > max_len ? max_len : _length;
    return std::string(_ptr + _index - len + 1, len);
}

std::string alx::strutil::right(const char* _ptr, size_t _len, size_t _index, size_t _length) {
    if (_index >= _len) return std::string();
    const size_t max_len = _len - _index;
    return std::string(_ptr + _index, _length <= max_len ? _length : max_len);
}

bool alx::strutil::check(const char* _ptr, size_t _len, const char* _tag, size_t _tag_len, size_t _start) {
    if (_len - _start < _tag_len) return false;
    return memcmp(_ptr + _start, _tag, _tag_len) == 0;
}

size_t alx::strutil::find(const char* _ptr, size_t _len, const char* _tag, size_t _tag_len, uint_64 _from, uint_64 _to) {
    return alx::find(_ptr, _len, _tag, _tag_len, _from, _to);
}

size_t alx::strutil::rfind(const char* _ptr, size_t _len, const char* _tag, size_t _tag_len, uint_64 _rfrom, uint_64 _rto) {
    return alx::rfind(_ptr, _len, _tag, _tag_len, _rfrom, _rto);
}

std::list<std::string> alx::strutil::split(const char* _ptr, size_t _len, const char* _tag, size_t _tag_len) {
    std::list<std::string> result;
    if (0 == _tag_len) {
        result.emplace_back(std::string());
        for (size_t i = 0; i < _len; ++i) result.emplace_back(right(_ptr, _len, i, 1));
        result.emplace_back(std::string());
        return result;
    }
    size_t index{0}, next{0};
    while ((next = strutil::find(_ptr, _len, _tag, _tag_len, index)) != uint_64_npos) {
        result.emplace_back(right(_ptr, _len, index, next - index));
        index = next + _tag_len;
    }
    result.emplace_back(right(_ptr, _len, index));
    return result;
}

std::string alx::strutil::join(const std::list<std::string>& _strl, const std::string& _tag) {
    std::string result;
    if (_strl.empty()) return result;
    if (_strl.size() == 1) return _strl.front();

    size_t len{0};
    for (const auto& s : _strl) len += s.size();
    len += _tag.size() * (_strl.size() - 1);
    result.reserve(len);

    auto iter = _strl.cbegin();
    result.append(*iter++);
    while (iter != _strl.cend())
        result.append(_tag), result.append(*iter++);

    return result;
}

std::vector<size_t> alx::strutil::compute_lps(const char* _str, size_t _len) {
    std::vector<size_t> lps(_len);
    size_t len = 0;
    size_t i = 1;
    while (i < _len) {
        if (_str[i] == _str[len]) {
            len++;
            lps[i] = len;
            i++;
        } else {
            if (0 != len) {
                len = lps[len - 1];
            } else {
                lps[i] = 0;
                i++;
            }
        }
    }
    return lps;
}

std::string alx::strutil::stringfy(const char* _ptr, size_t _len) {
    std::string result(_len + 2, '\"');
    memcpy(&result[1], _ptr, _len);
    return result;
}

size_t alx::strutil::remove_all(char* _ptr, size_t _len, char _ch) {
    size_t shift = 0;
    for (size_t i = 0; i < _len; ++i)
        if (_ptr[i] == _ch) ++shift;
        else if (shift > 0) _ptr[i - shift] = _ptr[i];
    return _len - shift;
}

std::string alx::strutil::to_utf8(uint_32 _cp) {
    if (_cp > 0x10FFFF || (_cp >= 0xD800 && _cp <= 0xDFFF))
        return std::string();

    std::string out;
    if (_cp <= 0x7F) {
        out += static_cast<char>(_cp);
    } else if (_cp <= 0x7FF) {
        out += static_cast<char>(0xC0 | (_cp >> 6));
        out += static_cast<char>(0x80 | (_cp & 0x3F));
    } else if (_cp <= 0xFFFF) {
        out += static_cast<char>(0xE0 | (_cp >> 12));
        out += static_cast<char>(0x80 | ((_cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (_cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (_cp >> 18));
        out += static_cast<char>(0x80 | ((_cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((_cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (_cp & 0x3F));
    }
    return out;
}

uint_32 alx::strutil::from_utf8(const std::string& _s, size_t _offset, size_t& _next) {
    if (_offset >= _s.size()) {
        _next = uint_64_npos;
        return 0;
    }

    const uint_8* p = reinterpret_cast<const uint_8*>(_s.data()) + _offset;
    const uint_8* end = reinterpret_cast<const uint_8*>(_s.data()) + _s.size();
    uint_32 cp;

    if ((*p & 0x80) == 0) {

        cp = *p++;
    } else if ((*p & 0xE0) == 0xC0 && end - p >= 2 &&
               (p[1] & 0xC0) == 0x80) {

        cp = ((uint_32) (p[0] & 0x1F) << 6) | (uint_32) (p[1] & 0x3F);
        if (cp < 0x80) {
            _next = uint_64_npos;
            return 0;
        }
        p += 2;
    } else if ((*p & 0xF0) == 0xE0 && end - p >= 3 &&
               (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {

        cp = ((uint_32) (p[0] & 0x0F) << 12) |
             ((uint_32) (p[1] & 0x3F) << 6) |
             (uint_32) (p[2] & 0x3F);
        if (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF)) {
            _next = uint_64_npos;
            return 0;
        }
        p += 3;
    } else if ((*p & 0xF8) == 0xF0 && end - p >= 4 &&
               (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 &&
               (p[3] & 0xC0) == 0x80) {

        cp = ((uint_32) (p[0] & 0x07) << 18) |
             ((uint_32) (p[1] & 0x3F) << 12) |
             ((uint_32) (p[2] & 0x3F) << 6) |
             (uint_32) (p[3] & 0x3F);
        if (cp < 0x10000 || cp > 0x10FFFF) {
            _next = uint_64_npos;
            return 0;
        }
        p += 4;
    } else {
        _next = uint_64_npos;
        return 0;
    }

    _next = static_cast<size_t>(p - reinterpret_cast<const uint_8*>(_s.data()));
    return cp;
}
