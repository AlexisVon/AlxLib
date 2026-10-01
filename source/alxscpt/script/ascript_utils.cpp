/*****************************************************************/ /**
 * \file   ascript_utils.cpp
 * \brief  Script utility functions — variant type conversion implementations
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#include "ascript_utils.h"
#include <cmath>
#include <stdexcept>

namespace alx {
    namespace script {

        // an existing directory is its own dirname; a file or a missing path gives its parent
        std::string dirname_of(const std::string& _path) {
            file_info fi(_path);
            return fi.is_dir() ? fi.path() : fi.get_parent().path();
        }

        int_64 cov_int(const variant& _v) {
            switch (_v.type()) {
            case variant::id<int_64>():
                return _v.to<int_64>();
            case variant::id<double>(): {

                // the cast is UB outside [-2^63, 2^63): NaN/Inf and such values are rejected
                double d = _v.to<double>();
                if (std::isnan(d) || std::isinf(d) || d >= 9223372036854775808.0 ||
                    d < -9223372036854775808.0)
                    throw script_exception{error_type::ConvError,
                                           std::string("Integer overflow: float out of int64 range")};
                return static_cast<int_64>(d);
            }
            case variant::id<bool>():
                return _v.to<bool>() ? 1 : 0;
            case variant::id<std::string>(): {
                const std::string& src = _v.to<std::string>();
                if (src.empty())
                    throw script_exception{error_type::ConvError, std::string("Cannot convert string to int")};
                int base = 10;
                size_t skip = 0;
                if (src.size() >= 2 && src[0] == '0') {
                    if (src[1] == 'x' || src[1] == 'X') {
                        base = 16;
                        skip = 2;
                    } else if (src[1] == 'o' || src[1] == 'O') {
                        base = 8;
                        skip = 2;
                    } else if (src[1] == 'b' || src[1] == 'B') {
                        base = 2;
                        skip = 2;
                    }
                }
                std::string s = src.substr(skip);
                if (s.empty())
                    throw script_exception{error_type::ConvError, std::string("Cannot convert string to int")};
                try {
                    size_t pos = 0;
                    int_64 val = std::stoll(s, &pos, base);
                    if (pos != s.size())
                        throw std::invalid_argument("partial");
                    return val;
                } catch (const std::out_of_range&) {
                    throw script_exception{error_type::ConvError, std::string("Integer overflow: " + src)};
                } catch (...) {
                }
                // both the "partial" marker and stoll()'s own rejection land here
                throw script_exception{error_type::ConvError, std::string("Invalid integer format: " + src)};
            }
            case variant::id<varvec>(): {
                auto& vec = _v.to<varvec>();
                if (vec.size() == 1) return cov_int(vec[0]);
                throw script_exception{error_type::ConvError, std::string("Cannot convert vec to int")};
            }
            case variant::id<varlst>(): {
                auto& lst = _v.to<varlst>();
                if (lst.size() == 1) return cov_int(lst.front());
                throw script_exception{error_type::ConvError, std::string("Cannot convert lst to int")};
            }
            // -1 is the null type; null yields the default value instead of throwing
            case -1:
                return 0;
            default:
                throw script_exception{error_type::ConvError, std::string("Cannot convert to int")};
            }
        }

        double cov_float(const variant& _v) {
            switch (_v.type()) {
            case variant::id<double>():
                return _v.to<double>();
            case variant::id<int_64>():
                return static_cast<double>(_v.to<int_64>());
            case variant::id<bool>():
                return _v.to<bool>() ? 1.0 : 0.0;
            case variant::id<std::string>(): {
                const std::string& src = _v.to<std::string>();

                if (src == "inf" || src == "+inf") return std::numeric_limits<double>::infinity();
                if (src == "-inf") return -std::numeric_limits<double>::infinity();
                if (src == "nan") return std::numeric_limits<double>::quiet_NaN();

                // stod() would take inf/nan spellings, hex floats and blanks; this screen stops those
                bool has_dot = false, has_e = false;
                for (size_t i = 0; i < src.size(); ++i) {
                    char c = src[i];
                    if (c >= '0' && c <= '9') continue;
                    if (c == '-' || c == '+') {
                        if (i != 0 && src[i - 1] != 'e' && src[i - 1] != 'E') goto fail;
                        continue;
                    }
                    if (c == '.' && !has_dot) {
                        has_dot = true;
                        continue;
                    }
                    if ((c == 'e' || c == 'E') && !has_e) {
                        has_e = true;
                        continue;
                    }
                    goto fail;
                }
                try {
                    double val = std::stod(src);
                    if (std::isinf(val) || std::isnan(val))
                        goto fail;
                    return val;
                } catch (...) {
                }
            fail:
                throw script_exception{error_type::ConvError, std::string("Cannot convert string to float")};
            }
            case variant::id<varvec>(): {
                auto& vec = _v.to<varvec>();
                if (vec.size() == 1) return cov_float(vec[0]);
                throw script_exception{error_type::ConvError, std::string("Cannot convert vec to float")};
            }
            case variant::id<varlst>(): {
                auto& lst = _v.to<varlst>();
                if (lst.size() == 1) return cov_float(lst.front());
                throw script_exception{error_type::ConvError, std::string("Cannot convert lst to float")};
            }
            case -1:
                return 0.0;
            default:
                throw script_exception{error_type::ConvError, std::string("Cannot convert to float")};
            }
        }

        // all-string concatenates, all-int encodes code points; a mixed sequence throws
        template <typename Seq>
        inline std::string cov_string_from_seq(const Seq& _seq, const char* _type_name) {
            if (_seq.empty()) return std::string();

            bool all_str = true;
            for (auto& x : _seq) {
                if (!x.template is<std::string>()) {
                    all_str = false;
                    break;
                }
            }
            if (all_str) {
                std::string result;
                for (auto& x : _seq) result += x.template to<std::string>();
                return result;
            }

            bool all_int = true;
            for (auto& x : _seq) {
                if (!x.template is<int_64>()) {
                    all_int = false;
                    break;
                }
            }
            if (all_int) {
                std::string result;
                for (auto& x : _seq) {
                    std::string u = strutil::to_utf8(static_cast<uint_32>(x.template to<int_64>()));
                    if (u.empty())
                        throw script_exception{error_type::ConvError, std::string("Invalid Unicode code point")};
                    result += u;
                }
                return result;
            }
            throw script_exception{error_type::ConvError,
                                   std::string("Cannot convert " + std::string(_type_name) + " to string")};
        }

        std::string cov_string(const variant& _v) {
            switch (_v.type()) {
            case variant::id<std::string>(): return _v.to<std::string>();
            case variant::id<bytes>(): {
                auto& b = _v.to<bytes>();
                return std::string(reinterpret_cast<const char*>(b.data()), b.size());
            }
            case variant::id<int_64>(): return std::to_string(_v.to<int_64>());
            case variant::id<double>(): return fmt_double(_v.to<double>());
            case variant::id<bool>(): return _v.to<bool>() ? "true" : "false";
            case variant::id<varvec>(): return cov_string_from_seq(_v.to<varvec>(), "vec");
            case variant::id<varlst>(): return cov_string_from_seq(_v.to<varlst>(), "lst");
            case -1:
                return std::string();
            default:
                throw script_exception{error_type::ConvError, std::string("Cannot convert to string")};
            }
        }

        bytes cov_bytes(const variant& _v) {
            switch (_v.type()) {
            case variant::id<bytes>():
                return _v.to<bytes>();
            case -1:
                return bytes();
            default:
                throw script_exception{error_type::ConvError, std::string("Cannot convert to bytes")};
            }
        }

        varvec cov_vec(const variant& _v) {
            switch (_v.type()) {
            case variant::id<varvec>():
                return _v.to<varvec>();
            case variant::id<varlst>(): {
                varvec result;
                for (auto& x : _v.to<varlst>()) result.push_back(x);
                return result;
            }
            case variant::id<std::string>(): {

                auto& s = _v.to<std::string>();
                varvec result;
                size_t next;
                for (size_t i = 0; i < s.size(); i = next) {
                    uint_32 cp = strutil::from_utf8(s, i, next);
                    if (next == uint_64_npos)
                        throw script_exception{error_type::ConvError, std::string("Invalid UTF-8 sequence in string")};
                    result.push_back(variant(static_cast<int_64>(cp)));
                }
                return result;
            }
            case variant::id<int_64>(): return varvec{variant(_v.to<int_64>())};
            case variant::id<double>(): return varvec{variant(_v.to<double>())};
            case variant::id<bool>(): return varvec{variant(_v.to<bool>())};
            case -1:
                return varvec();
            default:
                throw script_exception{error_type::ConvError, std::string("Cannot convert to vec")};
            }
        }

        varlst cov_lst(const variant& _v) {
            switch (_v.type()) {
            case variant::id<varlst>():
                return _v.to<varlst>();
            case variant::id<varvec>(): {
                varlst result;
                for (auto& x : _v.to<varvec>()) result.push_back(x);
                return result;
            }
            case variant::id<std::string>(): {

                auto& s = _v.to<std::string>();
                varlst result;
                size_t next;
                for (size_t i = 0; i < s.size(); i = next) {
                    uint_32 cp = strutil::from_utf8(s, i, next);
                    if (next == uint_64_npos)
                        throw script_exception{error_type::ConvError, std::string("Invalid UTF-8 sequence in string")};
                    result.push_back(variant(static_cast<int_64>(cp)));
                }
                return result;
            }
            case variant::id<int_64>(): {
                varlst result;
                result.push_back(variant(_v.to<int_64>()));
                return result;
            }
            case variant::id<double>(): {
                varlst result;
                result.push_back(variant(_v.to<double>()));
                return result;
            }
            case variant::id<bool>(): {
                varlst result;
                result.push_back(variant(_v.to<bool>()));
                return result;
            }
            case -1:
                return varlst();
            default:
                throw script_exception{error_type::ConvError, std::string("Cannot convert to lst")};
            }
        }

    }
}
