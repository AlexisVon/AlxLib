/*****************************************************************/ /**
 * \file   ascript_utils.h
 * \brief  Script utility functions — variant type conversion & helpers
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#ifndef _ALEXIS_SCRIPT_UTILS_H_
#define _ALEXIS_SCRIPT_UTILS_H_

#include "ascript.h"
#include "astring.h"
#include "afile.h"

namespace alx {
    namespace script {
        // Ordered (A, B) type-id pair packed into one switch key; the ids are small, 32 bits each
        template <typename A, typename B>
        constexpr uint_64 type_pair() {
            return (uint_64(variant::id<A>()) << 32) | uint_64(variant::id<B>());
        }

        inline uint_64 type_pair(const variant& _A, const variant& _B) {
            return (uint_64(_A.type()) << 32) | uint_64(_B.type());
        }

        // Containing directory of _path (itself when it is a directory); the result is file_info's absolute form
        std::string dirname_of(const std::string& _path);

        inline const char* type_name_script(const variant& _v) {
            switch (_v.type()) {
            case variant::id<int_64>(): return "int";
            case variant::id<double>(): return "float";
            case variant::id<bool>(): return "bool";
            case variant::id<std::string>(): return "string";
            case variant::id<bytes>(): return "bytes";
            case variant::id<varvec>(): return "vec";
            case variant::id<varmap>(): return "map";
            case variant::id<varlst>(): return "lst";
            case -1: return "null";
            default: return "?";
            }
        }

        // Strict readers: exact type only (an int is not a float here), except bool which also takes a number
        inline int_64 to_int_strict(const variant& _v) {
            if (_v.is<int_64>()) return _v.to<int_64>();
            throw script_exception{error_type::TypeError, std::string("Expected int, got ") + type_name_script(_v)};
        }

        inline double to_float_strict(const variant& _v) {
            if (_v.is<double>()) return _v.to<double>();
            throw script_exception{error_type::TypeError, std::string("Expected float, got ") + type_name_script(_v)};
        }

        inline bool to_bool_strict(const variant& _v) {
            switch (_v.type()) {
            case variant::id<bool>(): return _v.to<bool>();
            case variant::id<int_64>(): return _v.to<int_64>() != 0;
            case variant::id<double>(): return _v.to<double>() != 0.0;
            default:
                throw script_exception{error_type::TypeError, std::string("Expected bool or number, got ") + type_name_script(_v)};
            }
        }

        // Coercing conversions: null gives the zero value, anything else unconvertible throws ConvError
        int_64 cov_int(const variant& _v);
        double cov_float(const variant& _v);
        std::string cov_string(const variant& _v);
        bytes cov_bytes(const variant& _v);
        varvec cov_vec(const variant& _v);
        varlst cov_lst(const variant& _v);

        inline bool cov_bool(const variant& _v) {
            switch (_v.type()) {
            case variant::id<bool>(): return _v.to<bool>();
            case variant::id<int_64>(): return _v.to<int_64>() != 0;
            case variant::id<double>(): return _v.to<double>() != 0.0;
            case variant::id<std::string>(): return !_v.to<std::string>().empty();
            case variant::id<bytes>(): return !_v.to<bytes>().empty();
            case variant::id<varvec>(): return !_v.to<varvec>().empty();
            case variant::id<varlst>(): return !_v.to<varlst>().empty();
            case variant::id<varmap>(): return !_v.to<varmap>().empty();
            case -1: return false;
            default:
                throw script_exception{error_type::ConvError, std::string("Cannot convert to bool")};
            }
        }

        inline varmap cov_map(const variant& _v) {
            switch (_v.type()) {
            case variant::id<varmap>(): return _v.to<varmap>();
            case -1: return varmap();
            default:
                throw script_exception{error_type::ConvError, std::string("Cannot convert to map")};
            }
        }

        // Fixed 6 decimals, trailing zeros dropped but one kept (2.0, not 2); lossy past 6 decimals on purpose
        inline std::string fmt_double(double _v) {
            char buf[512];
            double mag = _v < 0 ? -_v : _v;

            // Below 5e-7 "%.6f" would print a nonzero magnitude as "0.0"; %g keeps it nonzero
            if (mag != 0.0 && mag < 0.0000005) {
                snprintf(buf, sizeof(buf), "%g", _v);
                return std::string(buf);
            }
            snprintf(buf, sizeof(buf), "%.6f", _v);
            std::string s(buf);
            size_t dot = s.find('.');
            if (dot == std::string::npos) return s;
            size_t end = s.size();
            while (end > dot + 2 && s[end - 1] == '0') end--;
            return s.substr(0, end);
        }

        // Diagnostic display form: containers collapse to tag + element count, not their contents
        inline std::string variant_to_display(const variant& _v) {
            switch (_v.type()) {
            case variant::id<std::string>(): return _v.to<std::string>();
            case variant::id<bytes>(): return "bytes(" + std::to_string(_v.to<bytes>().size()) + ")";
            case variant::id<int_64>(): return std::to_string(_v.to<int_64>());
            case variant::id<double>(): return fmt_double(_v.to<double>());
            case variant::id<bool>(): return _v.to<bool>() ? "true" : "false";
            case variant::id<varvec>(): return "[]:" + std::to_string(_v.to<varvec>().size());
            case variant::id<varlst>(): return "():" + std::to_string(_v.to<varlst>().size());
            case variant::id<varmap>(): return "{}:" + std::to_string(_v.to<varmap>().size());
            case -1: return "null";
            default: return "";
            }
        }

        // Mismatched types throw instead of yielding false; an int and a double do compare numerically
        inline bool eq_cmp(const variant& a, const variant& b) {
            if (a.null()) return b.null();
            if (b.null()) return false;
            switch (type_pair(a, b)) {
            case type_pair<int_64, int_64>():
                return a.to<int_64>() == b.to<int_64>();
            case type_pair<double, double>():
                return a.to<double>() == b.to<double>();
            case type_pair<int_64, double>():
            case type_pair<double, int_64>():
                return a.to_number() == b.to_number();
            case type_pair<std::string, std::string>():
                return a.to<std::string>() == b.to<std::string>();
            case type_pair<bool, bool>():
                return a.to<bool>() == b.to<bool>();
            case type_pair<bytes, bytes>():
                return a.to<bytes>() == b.to<bytes>();
            case type_pair<varvec, varvec>(): {
                auto& va = a.to<varvec>();
                auto& vb = b.to<varvec>();
                if (va.size() != vb.size()) return false;
                for (size_t i = 0; i < va.size(); i++)
                    if (!eq_cmp(va[i], vb[i])) return false;
                return true;
            }
            case type_pair<varlst, varlst>(): {
                auto& la = a.to<varlst>();
                auto& lb = b.to<varlst>();
                if (la.size() != lb.size()) return false;
                auto itb = lb.begin();
                for (auto& va : la) {
                    if (!eq_cmp(va, *itb)) return false;
                    ++itb;
                }
                return true;
            }
            case type_pair<varmap, varmap>(): {
                auto& ma = a.to<varmap>();
                auto& mb = b.to<varmap>();
                if (ma.size() != mb.size()) return false;
                for (auto it = ma.cbegin(); it != ma.cend(); ++it) {
                    auto found = mb.find(it.key());
                    if (found == mb.cend() || !eq_cmp(it.value(), found.value()))
                        return false;
                }
                return true;
            }
            default:
                throw script_exception{error_type::TypeError, std::string("Cannot compare different types")};
            }
        }

    }
}

#endif
