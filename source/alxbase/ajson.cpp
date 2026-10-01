/*****************************************************************/ /**
 * \file   ajson.cpp
 * \brief  JSON parser and generator
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "ajson.h"
#include "astream.h"
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#if __cplusplus >= 201703L && defined(__has_include)
#    if __has_include(<charconv>)
#        include <charconv>
#    endif
#endif

using namespace alx;

// std::stod throws on a value that overflows a double; a failed parse has to be the false return here
static bool parse_json_real(const std::string& _text, double& _value) {
    if (_text.empty()) return false;
    char* end{nullptr};
    errno = 0;
    const double value = strtod(_text.c_str(), &end);
    if (end == _text.c_str() || ERANGE == errno) return false;
    _value = value;
    return true;
}

static std::string json_double_text(double _v) {
    if (!std::isfinite(_v)) return "null";
    char buf[64];
    std::string s;
#if defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L
    auto res = std::to_chars(buf, buf + sizeof(buf), _v);
    s.assign(buf, res.ptr);
#else
    for (int prec = 1; prec <= 17; prec++) {
        snprintf(buf, sizeof(buf), "%.*g", prec, _v);
        s = buf;
        if (strtod(s.c_str(), nullptr) == _v) break;
    }
#endif
    // A bare integer would read back as int64 in the parser, so a double keeps a ".0"
    if (s.find_first_of(".eE") == std::string::npos) s += ".0";
    return s;
}

varmap alx::json_object::to_varmap() const {
    varmap result;
    for (const auto& it : *this) result.insert(it.first, it.second->to_variant());
    return result;
}

varmap alx::json_object::take_varmap() {
    varmap result;
    for (const auto& it : *this) result.insert(it.first, it.second->take_variant());
    return result;
}

json_object alx::json_object::from_varmap(const varmap& _vmap) {
    json_object result;
    for (const auto& it : _vmap) result.insert(it.first, json_value::from_variant(*it.second));
    return result;
}

json_object alx::json_object::from_varmap(varmap&& _vmap) {
    json_object result;
    for (const auto& it : _vmap) result.insert(it.first, json_value::from_variant(std::move(*it.second)));
    return result;
}

varvec alx::json_array::to_varvec() const {
    varvec result;
    for (const auto& it : *this) result.push_back(it->to_variant());
    return result;
}

varvec alx::json_array::take_varvec() {
    varvec result;
    for (const auto& it : *this) result.push_back(it->take_variant());
    return result;
}

varlst alx::json_array::to_varlst() const {
    varlst result;
    for (const auto& it : *this) result.push_back(it->to_variant());
    return result;
}

varlst alx::json_array::take_varlst() {
    varlst result;
    for (const auto& it : *this) result.push_back(it->take_variant());
    return result;
}

json_array alx::json_array::from_varvec(const varvec& _vvec) {
    json_array result;
    for (const auto& it : _vvec) result.append(json_value::from_variant(it));
    return result;
}

json_array alx::json_array::from_varvec(varvec&& _vvec) {
    json_array result;
    for (variant& it : _vvec) result.append(json_value::from_variant(std::move(it)));
    return result;
}

json_array alx::json_array::from_varlst(const varlst& _vlst) {
    json_array result;
    for (const auto& it : _vlst) result.append(json_value::from_variant(it));
    return result;
}

json_array alx::json_array::from_varlst(varlst&& _vlst) {
    json_array result;
    for (variant& it : _vlst) result.append(json_value::from_variant(std::move(it)));
    return result;
}

#define CONVER(TYPE, _DF, _OK) \
    ((nullptr == _OK ? is<TYPE>() : (*_OK = is<TYPE>())) ? to<TYPE>(_DF) : _DF)

long long& alx::json_value::to_intg(long long& _def, bool* _ok) { return CONVER(long long, _def, _ok); }
bool& alx::json_value::to_bool(bool& _def, bool* _ok) { return CONVER(bool, _def, _ok); }
double& alx::json_value::to_double(double& _def, bool* _ok) { return CONVER(double, _def, _ok); }
std::string& alx::json_value::to_string(std::string& _def, bool* _ok) { return CONVER(std::string, _def, _ok); }
json_object& alx::json_value::to_object(json_object& _def, bool* _ok) { return CONVER(json_object, _def, _ok); }
json_array& alx::json_value::to_array(json_array& _def, bool* _ok) { return CONVER(json_array, _def, _ok); }

long long json_value::to_intg(const long long& _def, bool* _ok) const { return CONVER(long long, _def, _ok); }
bool json_value::to_bool(const bool& _def, bool* _ok) const { return CONVER(bool, _def, _ok); }
double json_value::to_double(const double& _def, bool* _ok) const { return CONVER(double, _def, _ok); }
const std::string& json_value::to_string(const std::string& _def, bool* _ok) const { return CONVER(std::string, _def, _ok); }
const json_array& json_value::to_array(const json_array& _def, bool* _ok) const { return CONVER(json_array, _def, _ok); }
const json_object& json_value::to_object(const json_object& _def, bool* _ok) const { return CONVER(json_object, _def, _ok); }

variant alx::json_value::to_variant() const {
    variant result;
    switch (type()) {
    case id<long long>(): {
        result = to<long long>();
        break;
    }
    case id<bool>(): {
        result = to<bool>();
        break;
    }
    case id<double>(): {
        result = to<double>();
        break;
    }
    case id<std::string>(): {
        result = to<std::string>();
        break;
    }
    case id<json_object>(): {
        result = to_object().to_varmap();
        break;
    }
    case id<json_array>(): {
        result = to_array().to_varvec();
        break;
    }
    }
    return result;
}

variant alx::json_value::take_variant() {
    variant result;
    switch (type()) {
    case id<std::string>(): {
        result = std::move(as<std::string>());
        break;
    }
    case id<json_object>(): {
        result = as<json_object>().take_varmap();
        break;
    }
    case id<json_array>(): {
        result = as<json_array>().take_varvec();
        break;
    }
    default: {
        result = to_variant();
        break;
    }
    }
    return result;
}

json_value alx::json_value::from_variant(const variant& _var) {
#define CASE_SAMPLE_TYPE(F, T) \
    case variant::id<F>(): return json_value(std::is_same_v<F, T> ? _var.to<F>() : (T) (_var.to<F>()))
#define CASE_LIST_TYPE(F, T) \
    case variant::id<std::list<F>>(): return json_array::from_list<F, T>(_var.to<std::list<F>>())
#define CASE_VECTOR_TYPE(F, T) \
    case variant::id<std::vector<F>>(): return json_array::from_vector<F, T>(_var.to<std::vector<F>>())
#define CASE_TYPE(F, T)     \
    CASE_SAMPLE_TYPE(F, T); \
    CASE_LIST_TYPE(F, T);   \
    CASE_VECTOR_TYPE(F, T)

    switch (_var.type()) {
        CASE_TYPE(bool, bool);
        CASE_TYPE(char, long long);
        CASE_TYPE(unsigned char, long long);
        CASE_TYPE(short, long long);
        CASE_TYPE(unsigned short, long long);
        CASE_TYPE(int, long long);
        CASE_TYPE(unsigned int, long long);
        CASE_TYPE(long long, long long);
        CASE_TYPE(unsigned long long, long long);
        CASE_TYPE(float, double);
        CASE_TYPE(double, double);
        CASE_TYPE(std::string, std::string);
        CASE_TYPE(bytes, bytes);

    case variant::id<varvec>(): return json_array::from_varvec(_var.to<varvec>());
    case variant::id<varlst>(): return json_array::from_varlst(_var.to<varlst>());
    case variant::id<varmap>(): return json_object::from_varmap(_var.to<varmap>());
    default: return json_value();
    }
}

json_value alx::json_value::from_variant(variant&& _var) {
    switch (_var.type()) {

    case variant::id<std::string>(): return json_value(std::move(_var.as<std::string>()));
    case variant::id<std::vector<std::string>>(): return json_array::from_vector<std::string, std::string>(std::move(_var.as<std::vector<std::string>>()));
    case variant::id<std::list<std::string>>(): return json_array::from_list<std::string, std::string>(std::move(_var.as<std::list<std::string>>()));
    case variant::id<varvec>(): return json_array::from_varvec(std::move(_var.as<varvec>()));
    case variant::id<varlst>(): return json_array::from_varlst(std::move(_var.as<varlst>()));
    case variant::id<varmap>(): return json_object::from_varmap(std::move(_var.as<varmap>()));
    default: return from_variant(_var);
    }
}

json_array::json_array(std::initializer_list<json_value> _list) {
    for (auto& it : _list) push_back(new json_value(it));
}

alx::json_array::json_array(const json_array& _value)
    : vector() {
    reserve(_value.size());
    for (const json_value* it : _value) push_back(new json_value(*it));
}

alx::json_array::json_array(json_array&& _value) noexcept
    : vector() {
    vector::swap(_value);
}

alx::json_array& alx::json_array::operator=(const json_array& _value) {
    if (this == &_value) return *this;
    json_array hold(_value);
    vector::swap(hold);
    return *this;
}

alx::json_array& alx::json_array::operator=(json_array&& _value) noexcept {
    if (this == &_value) return *this;
    clear();
    vector::swap(_value);
    return *this;
}

alx::json_array::~json_array() {
    clear();
}

void alx::json_array::clear() {
    for (const json_value* it : *this) delete it;
    vector::clear();
}

void alx::json_array::append(json_value&& _value) { push_back(new json_value(std::forward<json_value>(_value))); }
void alx::json_array::append(const json_value& _value) { push_back(new json_value(_value)); }

bool alx::json_array::operator==(const json_array& _value) const {
    if (size() != _value.size()) return false;
    for (size_t i = 0; i < size(); i++)
        if (*vector::operator[](i) != *_value.vector::operator[](i)) return false;
    return true;
}

bool alx::json_array::operator!=(const json_array& _value) const {
    return !(*this == _value);
}

namespace alx {
    namespace {
        namespace json_doc_impl {

            // Writes at most 6 bytes into _dst, the widest escape being the four-hex-digit control form
            inline size_t json_escape_put(const char _ch, char* _dst) {
                switch (_ch) {
                case '"': memcpy(_dst, "\\\"", 2); return 2;
                case '\\': memcpy(_dst, "\\\\", 2); return 2;
                case '\b': memcpy(_dst, "\\b", 2); return 2;
                case '\f': memcpy(_dst, "\\f", 2); return 2;
                case '\n': memcpy(_dst, "\\n", 2); return 2;
                case '\r': memcpy(_dst, "\\r", 2); return 2;
                case '\t': memcpy(_dst, "\\t", 2); return 2;
                default: break;
                }
                if ((uint_8) _ch >= 0X20) return 0;
                static const char hex[] = "0123456789abcdef";
                _dst[0] = '\\';
                _dst[1] = 'u';
                _dst[2] = '0';
                _dst[3] = '0';
                _dst[4] = hex[((uint_8) _ch >> 4) & 0X0F];
                _dst[5] = hex[(uint_8) _ch & 0X0F];
                return 6;
            }

            // In place is safe: an escape decodes to no more bytes than it spans, so _write never passes _read
            inline void json_descape_conv(const char*& _read, char*& _write, const char* _end) {
                const char* read = _read + 1;
                const char esc = *read++;
                switch (esc) {
                case '"': *_write++ = '"'; break;
                case '\\': *_write++ = '\\'; break;
                case 'b': *_write++ = '\b'; break;
                case 'f': *_write++ = '\f'; break;
                case 'n': *_write++ = '\n'; break;
                case 'r': *_write++ = '\r'; break;
                case 't': *_write++ = '\t'; break;
                case 'u': {
                    if (read + 3 < _end) {
                        unsigned int unicode = 0;
                        bool valid = true;
                        for (int k = 0; k < 4; k++) {
                            const char h = read[k];
                            unicode <<= 4;
                            if (h >= '0' && h <= '9') unicode |= (h - '0');
                            else if (h >= 'A' && h <= 'F') unicode |= (h - 'A' + 10);
                            else if (h >= 'a' && h <= 'f') unicode |= (h - 'a' + 10);
                            else {
                                valid = false;
                                break;
                            }
                        }
                        if (valid) {
                            read += 4;
                            if (unicode <= 0X7F) *_write++ = (char) unicode;
                            else if (unicode <= 0X7FF) {
                                *_write++ = (char) (0XC0 | (unicode >> 6));
                                *_write++ = (char) (0X80 | (unicode & 0X3F));
                            } else if (unicode <= 0XFFFF) {
                                *_write++ = (char) (0XE0 | (unicode >> 12));
                                *_write++ = (char) (0X80 | ((unicode >> 6) & 0X3F));
                                *_write++ = (char) (0X80 | (unicode & 0X3F));
                            }
                        } else *_write++ = 'u';
                    } else *_write++ = 'u';
                    break;
                }
                default: *_write++ = esc; break;
                }
                _read = read;
            }

            inline bool json_sink_put(std::string& _dst, const char* _ptr, const size_t _size) {
                _dst.append(_ptr, _size);
                return true;
            }
            inline bool json_sink_put(ostream& _dst, const char* _ptr, const size_t _size) { return _dst.append(_ptr, _size); }
            inline bool json_sink_put(std::string& _dst, const char _ch) {
                _dst += _ch;
                return true;
            }
            inline bool json_sink_put(ostream& _dst, const char _ch) { return _dst.append(&_ch, 1); }
            inline bool json_sink_put(std::string& _dst, const char* _text) {
                _dst += _text;
                return true;
            }
            inline bool json_sink_put(ostream& _dst, const char* _text) { return _dst.append(_text, strlen(_text)); }
            inline bool json_sink_put(std::string& _dst, const std::string& _text) {
                _dst += _text;
                return true;
            }
            inline bool json_sink_put(ostream& _dst, const std::string& _text) { return _dst.append(_text.data(), _text.size()); }

            template <typename _SINK, size_t _N> inline bool json_sink_put(_SINK& _dst, const char (&_text)[_N]) {
                return json_sink_put(_dst, _text, _N - 1);
            }

            template <typename _SINK> inline bool escape_into(const std::string& _src, _SINK& _dst) {
                char buf[6];
                size_t from = 0;
                for (size_t i = 0; i < _src.size(); i++) {
                    const size_t len = json_escape_put(_src[i], buf);
                    if (0 == len) continue;
                    if (!json_sink_put(_dst, _src.data() + from, i - from)) return false;
                    if (!json_sink_put(_dst, buf, len)) return false;
                    from = i + 1;
                }
                return json_sink_put(_dst, _src.data() + from, _src.size() - from);
            }

            // The forwarding reference keeps const-ness, so one body serves both a read of a const
            // tree and a consume of an rvalue one, where drop()/drain() hand the payloads over
            class writer {
            public:
                template <typename NODE, typename _SINK> inline static bool value_compact(NODE&& _value, _SINK& _out);
                template <typename NODE, typename _SINK> inline static bool object_compact(NODE&& _json, _SINK& _out);
                template <typename NODE, typename _SINK> inline static bool array_compact(NODE&& _array, _SINK& _out);
                template <typename NODE, typename _SINK> inline static bool value_incompact(NODE&& _value, int _level, _SINK& _out);
                template <typename NODE, typename _SINK> inline static bool object_incompact(NODE&& _json, int _level, _SINK& _out);
                template <typename NODE, typename _SINK> inline static bool array_incompact(NODE&& _array, int _level, _SINK& _out);

            private:

                inline static const std::string& text_of(const json_value& _value) { return _value.to_string(); }
                inline static std::string& text_of(json_value& _value) { return _value.as<std::string>(); }
                inline static const json_object& object_of(const json_value& _value) { return _value.to<json_object>(); }
                inline static json_object& object_of(json_value& _value) { return _value.as<json_object>(); }
                inline static const json_array& array_of(const json_value& _value) { return _value.to<json_array>(); }
                inline static json_array& array_of(json_value& _value) { return _value.as<json_array>(); }

                inline static void drop(const std::string&) {}
                inline static void drop(std::string& _str) { std::string().swap(_str); }
                inline static void drop(const json_object&, json_object::const_iterator& _it) { ++_it; }
                inline static void drop(json_object& _json, json_object::iterator& _it) {
                    // The loop always sits on the first entry, and erase() kills the iterator it is given
                    _json.erase(_it.key());
                    _it = _json.begin();
                }
                inline static void drop(const json_array&, size_t) {}
                inline static void drop(json_array& _array, size_t _index) {

                    // The slot is emptied in place because erasing from the front is quadratic; drain() clears the holes
                    json_value*& slot = static_cast<std::vector<json_value*>&>(_array)[_index];
                    delete slot;
                    slot = nullptr;
                }
                inline static void drain(const json_array&) {}
                inline static void drain(json_array& _array) { _array.clear(); }

                template <typename _SINK> inline static bool text(const std::string& _str, _SINK& _out);

                inline static bool level_put(std::string& _str, int _level);
                inline static bool level_put(ostream& _str, int _level);
                template <typename _SINK> inline static bool level_end(_SINK& _str, const char _ch, int _level);
                template <typename _SINK> inline static bool level_key(_SINK& _str, const std::string& _key, int _level);
            };

            template <typename _SINK> inline bool writer::text(const std::string& _str, _SINK& _out) {
                return json_sink_put(_out, '"') && escape_into(_str, _out) && json_sink_put(_out, '"');
            }

            inline bool writer::level_put(std::string& _str, int _level) {

                const size_t _len = _str.length();
                _str.resize(_len + _level + 1, '\t');
                _str[_len] = '\n';
                return true;
            }

            inline bool writer::level_put(ostream& _str, int _level) {
                static const char tabs[] = "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t";
                if (!json_sink_put(_str, '\n')) return false;
                for (int done = 0; done < _level;) {
                    const size_t chunk = std::min((size_t) (_level - done), sizeof(tabs) - 1);
                    if (!_str.append(tabs, chunk)) return false;
                    done += (int) chunk;
                }
                return true;
            }

            template <typename _SINK> inline bool writer::level_end(_SINK& _str, const char _ch, int _level) {

                return level_put(_str, _level) && json_sink_put(_str, _ch);
            }

            template <typename _SINK> inline bool writer::level_key(_SINK& _str, const std::string& _key, int _level) {

                return level_put(_str, _level) && text(_key, _str) && json_sink_put(_str, ": ");
            }

            template <typename NODE, typename _SINK> inline bool writer::value_compact(NODE&& _value, _SINK& _out) {
                switch (_value.type()) {
                case json_value::id<long long>(): return json_sink_put(_out, std::to_string(_value.to_intg()));
                case json_value::id<double>(): return json_sink_put(_out, json_double_text(_value.to_double()));
                case json_value::id<bool>(): return json_sink_put(_out, _value.to_bool() ? "true" : "false");
                case json_value::id<std::string>(): {
                    auto&& str = text_of(_value);
                    const bool ok = text(str, _out);
                    drop(str);
                    return ok;
                }
                case json_value::id<json_array>(): return array_compact(array_of(_value), _out);
                case json_value::id<json_object>(): return object_compact(object_of(_value), _out);
                default: return json_sink_put(_out, _value.is_null() ? "null" : "\"unknown\"");
                }
            }

            template <typename NODE, typename _SINK> inline bool writer::object_compact(NODE&& _json, _SINK& _out) {
                if (!json_sink_put(_out, '{')) return false;
                bool _first{true};
                for (auto it = _json.begin(); it != _json.end();) {
                    if (!_first) {
                        if (!json_sink_put(_out, ',')) return false;
                    } else _first = false;
                    if (!text(it.key(), _out)) return false;
                    if (!json_sink_put(_out, ':')) return false;
                    if (!value_compact(it.value(), _out)) return false;
                    drop(_json, it);
                }
                return json_sink_put(_out, '}');
            }

            template <typename NODE, typename _SINK> inline bool writer::array_compact(NODE&& _array, _SINK& _out) {
                if (!json_sink_put(_out, '[')) return false;
                for (size_t i = 0; i < _array.size(); i++) {
                    if (i && !json_sink_put(_out, ',')) return false;
                    if (!value_compact(_array[i], _out)) return false;
                    drop(_array, i);
                }
                drain(_array);
                return json_sink_put(_out, ']');
            }

            template <typename NODE, typename _SINK> inline bool writer::value_incompact(NODE&& _value, int _level, _SINK& _out) {
                switch (_value.type()) {
                case json_value::id<long long>(): return json_sink_put(_out, std::to_string(_value.to_intg()));
                case json_value::id<double>(): return json_sink_put(_out, json_double_text(_value.to_double()));
                case json_value::id<bool>(): return json_sink_put(_out, _value.to_bool() ? "true" : "false");
                case json_value::id<std::string>(): {
                    auto&& str = text_of(_value);
                    const bool ok = text(str, _out);
                    drop(str);
                    return ok;
                }
                case json_value::id<json_array>(): return array_incompact(array_of(_value), _level, _out);
                case json_value::id<json_object>(): return object_incompact(object_of(_value), _level, _out);
                default: return json_sink_put(_out, _value.is_null() ? "null" : "\"unknown\"");
                }
            }

            template <typename NODE, typename _SINK> inline bool writer::object_incompact(NODE&& _json, int _level, _SINK& _out) {
                if (_json.empty()) return json_sink_put(_out, "{}");
                if (!json_sink_put(_out, '{')) return false;
                bool _first{true};
                for (auto it = _json.begin(); it != _json.end();) {
                    if (!_first) {
                        if (!json_sink_put(_out, ',')) return false;
                    } else _first = false;
                    if (!level_key(_out, it.key(), _level + 1)) return false;
                    if (!value_incompact(it.value(), _level + 1, _out)) return false;
                    drop(_json, it);
                }
                return level_end(_out, '}', _level);
            }

            template <typename NODE, typename _SINK> inline bool writer::array_incompact(NODE&& _array, int _level, _SINK& _out) {
                if (_array.empty()) return json_sink_put(_out, "[]");
                if (!json_sink_put(_out, '[')) return false;
                for (size_t i = 0; i < _array.size(); i++) {
                    if (i && !json_sink_put(_out, ',')) return false;
                    if (!level_put(_out, _level + 1)) return false;
                    if (!value_incompact(_array[i], _level + 1, _out)) return false;
                    drop(_array, i);
                }
                if (!level_end(_out, ']', _level)) return false;
                drain(_array);
                return true;
            }
        }
    }
}

std::string alx::json_doc::to_json(const json_object& _json, bool _compact) {
    std::string result;
    _compact ? json_doc_impl::writer::object_compact(_json, result) : json_doc_impl::writer::object_incompact(_json, 0, result);
    return result;
}

std::string alx::json_doc::to_json(json_object&& _json, bool _compact) {
    std::string result;
    _compact ? json_doc_impl::writer::object_compact(std::move(_json), result)
             : json_doc_impl::writer::object_incompact(std::move(_json), 0, result);
    return result;
}

std::string alx::json_doc::to_string(const json_value& _json, bool _compact) {
    std::string result;
    if (_json.is_object()) {
        _compact ? json_doc_impl::writer::object_compact(_json.to_object(), result)
                 : json_doc_impl::writer::object_incompact(_json.to_object(), 0, result);
    } else if (_json.is_array()) {
        _compact ? json_doc_impl::writer::array_compact(_json.to_array(), result)
                 : json_doc_impl::writer::array_incompact(_json.to_array(), 0, result);
    } else {
        json_doc_impl::writer::value_compact(_json, result);
    }
    return result;
}

std::string alx::json_doc::to_string(json_value&& _json, bool _compact) {
    std::string result;
    _compact ? json_doc_impl::writer::value_compact(std::move(_json), result)
             : json_doc_impl::writer::value_incompact(std::move(_json), 0, result);
    return result;
}

bool alx::json_doc::to_json(const json_object& _json, ostream& _out, bool _compact) {
    return _compact ? json_doc_impl::writer::object_compact(_json, _out)
                    : json_doc_impl::writer::object_incompact(_json, 0, _out);
}

bool alx::json_doc::to_json(json_object&& _json, ostream& _out, bool _compact) {
    return _compact ? json_doc_impl::writer::object_compact(std::move(_json), _out)
                    : json_doc_impl::writer::object_incompact(std::move(_json), 0, _out);
}

bool alx::json_doc::to_string(const json_value& _json, ostream& _out, bool _compact) {
    if (_json.is_object()) {
        return _compact ? json_doc_impl::writer::object_compact(_json.to_object(), _out)
                        : json_doc_impl::writer::object_incompact(_json.to_object(), 0, _out);
    }
    if (_json.is_array()) {
        return _compact ? json_doc_impl::writer::array_compact(_json.to_array(), _out)
                        : json_doc_impl::writer::array_incompact(_json.to_array(), 0, _out);
    }
    return _compact ? json_doc_impl::writer::value_compact(_json, _out)
                    : json_doc_impl::writer::value_incompact(_json, 0, _out);
}

bool alx::json_doc::to_string(json_value&& _json, ostream& _out, bool _compact) {
    return _compact ? json_doc_impl::writer::value_compact(std::move(_json), _out)
                    : json_doc_impl::writer::value_incompact(std::move(_json), 0, _out);
}

json_object alx::json_doc::from_json(const char* _data, size_t _size, bool* _ok) {
    json_object result;
    bool ok = syn_json(_data, _size, result);
    if (_ok) *_ok = ok;
    // A failed parse can leave the entries it already inserted; the promised return is an empty tree
    return ok ? result : (result.clear(), result);
}

json_value alx::json_doc::from_value(const char* _data, size_t _size, bool* _ok) {
    json_value result;
    bool ok = syn_json_value(_data, _size, result);
    if (_ok) *_ok = ok;
    return ok ? result : json_value();
}

inline bool alx::json_doc::syn_json(const char* _data, size_t _size, json_object& _json) {
    size_t next{0};
    json_token begin = lex_json(_data, _size, next, next);
    if ((_size == 0) || begin.type != LCURLY) return false;
    else {
        return syn_json_object(_data, _size, next, next, _json);
    }
}

inline bool alx::json_doc::syn_json_value(const char* _data, size_t _size, json_value& _val) {
    size_t next{0};
    json_token begin = lex_json(_data, _size, next, next);
    if (_size == 0) return false;
    if (begin.type == LCURLY) {
        json_object obj;
        if (syn_json_object(_data, _size, next, next, obj)) {
            _val = json_value(std::move(obj));
            return true;
        }
        return false;
    } else if (begin.type == LSQUAR) {
        json_array arr;
        if (syn_json_array(_data, _size, next, next, arr)) {
            _val = json_value(std::move(arr));
            return true;
        }
        return false;
    } else if (begin.type == VALUE_) {
        _val = begin.value;
        return true;
    }
    return false;
}

inline bool alx::json_doc::syn_json_array(const char* _data, size_t _size, const size_t _from, size_t& _next, json_array& _array) {
    _next = _from;
    enum state { VAL,
                 COM };
    state req = VAL;
    while (true) {
        const json_token cur = lex_json(_data, _size, _next, _next);
        if (cur.type == ERROR_) return false;
        if (cur.type == RSQUAR) return true;
        switch (req) {
        // Deliberate fallthrough: every token lands back in VAL, which is what lets a trailing comma through
        case VAL:
            req = COM;
            if (cur.type == VALUE_) _array.append(cur.value);
            else if (cur.type == LCURLY) {
                json_object json;
                if (syn_json_object(_data, _size, _next, _next, json)) {
                    _array.append(std::move(json));
                    continue;
                }
            } else if (cur.type == LSQUAR) {
                json_array array;
                if (syn_json_array(_data, _size, _next, _next, array)) {
                    _array.append(std::move(array));
                    continue;
                }
            }
        case COM: req = VAL; continue;
        default: return false;
        }
    }
    return false;
}

inline bool alx::json_doc::syn_json_object(const char* _data, size_t _size, const size_t _from, size_t& _next, json_object& _json) {
    _next = _from;
    enum state { KEY = 0,
                 COL,
                 VAL,
                 COM };
    state req = KEY;
    std::string key;
    json_value value;
    while (true) {
        json_token cur = lex_json(_data, _size, _next, _next);
        if (cur.type == ERROR_) return false;
        if (cur.type == RCURLY) {
            // Only after a value: an empty object and a trailing comma arrive with nothing pending
            if (req == COM) _json.insert(key, value);
            return true;
        }
        switch (req) {
        case KEY:
            req = COL;
            if (cur.type == VALUE_ && cur.value.is<std::string>()) {
                key = cur.value.to_string();
                continue;
            } else return false;
        case COL:
            req = VAL;
            if (cur.type == COLON_) continue;
            else return false;
        case VAL:
            req = COM;
            if (cur.type == VALUE_) {
                value = std::move(cur.value);
                continue;
            } else if (cur.type == LCURLY) {
                json_object obj;
                if (syn_json_object(_data, _size, _next, _next, obj)) {
                    value = std::move(obj);
                    continue;
                } else return false;
            } else if (cur.type == LSQUAR) {
                json_array arr;
                if (syn_json_array(_data, _size, _next, _next, arr)) {
                    value = std::move(arr);
                    continue;
                } else return false;
            } else return false;
        case COM:
            req = KEY;
            if (cur.type == COMMA_) {
                _json.insert(key, std::move(value));
                continue;
            } else return false;
        default:
            return false;
        }
    }
}

inline json_doc::json_token alx::json_doc::lex_json(const char* _data, size_t _size, size_t _index, size_t& _next) {
    _next = _index;
    while (_next < _size) {
        switch (_data[_next]) {
        case '\t':
        case '\n':
        case '\r':
        case ' ': _next++; continue;
        case '{': _next++; return {LCURLY};
        case '}': _next++; return {RCURLY};
        case '[': _next++; return {LSQUAR};
        case ']': _next++; return {RSQUAR};
        case ':': _next++; return {COLON_};
        case ',': _next++; return {COMMA_};
        default: {
            // The value lexer stops on the last character of the value, so the else below steps over it
            json_value value = lex_json_value(_data, _size, _next, _next);
            if (_next == uint_64_npos) return {ERROR_};
            else _next++;
            return {VALUE_, value};
        }
        }
    }
    return {ERROR_};
}

inline json_value alx::json_doc::lex_json_value(const char* _data, size_t _size, size_t _index, size_t& _next) {
    switch (_data[_index]) {
    case '"': {
        _next = _index + 1;
        while (_next != uint_64_npos) {
            const uint_8 quote = '"';
            _next = alx::find_func::find_by_val<uint_8>(_data, &quote, _next, _size);
            if (_next == uint_64_npos) break;
            // The backward scan stops at the opening quote, which is not a backslash, so loc never wraps around
            size_t cnt{0}, loc = _next;
            while (--loc >= 0) {
                if (_data[loc] == '\\') cnt++;
                else break;
            }
            if (0 == cnt || (cnt & 0X01U) == 0) break;
            else _next++;
        }
        if (_next == uint_64_npos) return json_value();
        std::string str = strutil::right(_data, _size, _index + 1, _next - _index - 1);
        return std::move((descape(str, str, 0), str));
    }
    case 't': {
        if (strutil::check(_data, _size, "true", _index)) {
            _next = _index + 3;
            return true;
        } else {
            _next = uint_64_npos;
            return json_value();
        }
    }
    case 'f': {
        if (strutil::check(_data, _size, "false", _index)) {
            _next = _index + 4;
            return false;
        } else {
            _next = uint_64_npos;
            return json_value();
        }
    }
    case 'n': {
        if (strutil::check(_data, _size, "null", _index)) {
            _next = _index + 3;
            return json_value();
        } else {
            _next = uint_64_npos;
            return json_value();
        }
    }
    default: return lex_json_number(_data, _size, _index, _next);
    }
}

inline json_value alx::json_doc::lex_json_number(const char* _data, size_t _size, size_t _index, size_t& _next) {
    enum state : int { START = 0,
                       INTEG,
                       SIGNT,
                       DECIM,
                       FRACT,
                       EXPON,
                       EXPSI,
                       EXPNU,
                       ERROR };
    enum symbl : int { NUM = 0,
                       SIG,
                       DEC,
                       EXP,
                       ERR };
    static state table[ERROR][ERR]{

        {INTEG, SIGNT, ERROR, ERROR},
        {INTEG, ERROR, DECIM, EXPON},
        {INTEG, ERROR, ERROR, ERROR},
        {FRACT, ERROR, ERROR, ERROR},
        {FRACT, ERROR, ERROR, EXPON},
        {EXPNU, EXPSI, ERROR, ERROR},
        {EXPNU, ERROR, ERROR, ERROR},
        {EXPNU, ERROR, ERROR, ERROR},
    };
    state now{START};
    for (size_t i = _index; i < _size; i++) {
        const char current = _data[i];
        const symbl crtsym =
            current >= '0' && current <= '9' ? NUM : current == '.'                 ? DEC
                                                 : current == '+' || current == '-' ? SIG
                                                 : current == 'E' || current == 'e' ? EXP
                                                                                    : ERR;
        const state next = crtsym == ERR ? ERROR : table[now][crtsym];
        if (next == ERROR || i == _size - 1) {
            // A number cut off by a bad character keeps the state it was in and stops before that
            // character, which the caller then lexes on its own
            if (next == ERROR) _next = i - 1;
            else _next = i;

            switch (next == ERROR ? now : next) {
            case INTEG: {
                std::string num_str = strutil::right(_data, _size, _index, i - _index + 1);
                char* end;
                errno = 0;
                long long val = strtoll(num_str.c_str(), &end, 10);
                if (errno != ERANGE) return val;
                double real{0};
                if (!parse_json_real(num_str, real)) { _next = uint_64_npos; return json_value(); }
                return real;
            }
            case FRACT:
            case EXPNU: {
                double real{0};
                if (!parse_json_real(strutil::right(_data, _size, _index, i - _index + 1), real)) {
                    _next = uint_64_npos;
                    return json_value();
                }
                return real;
            }
            default: _next = uint_64_npos; return json_value();
            }
        }
        now = next;
    }
    _next = uint_64_npos;
    return json_value();
}

void alx::json_doc::escape(const std::string& _src, std::string& _dst, size_t _ofst) {
    char buf[6];
    if (&_src != &_dst) {
        _dst.resize(_ofst);
        json_doc_impl::escape_into(_src, _dst);
        return;
    }

    // Same string on both sides: count the growth, resize once, then write from the back so unread input is never clobbered
    const size_t size = _src.size();
    size_t count = 0;
    for (size_t i = 0; i < size; i++) {
        const size_t len = json_doc_impl::json_escape_put(_src[i], buf);
        if (len) count += len - 1;
    }
    if (0 == count) return;
    _dst.resize(size + count);

    const char* read = _dst.data() + size;
    char* write = _dst.data() + size + count;
    while (read != _dst.data()) {
        const size_t len = json_doc_impl::json_escape_put(*--read, buf);
        if (len) {
            write -= len;
            memcpy(write, buf, len);
        } else *--write = *read;
    }
}

void alx::json_doc::descape(const std::string& _src, std::string& _dst, size_t _ofst) {
    const size_t size = _src.size();
    _dst.resize(_ofst + size);

    const char* read = _src.data();
    const char* const end = read + size;
    char* write = _dst.data() + _ofst;

    while (read < end) {
        if (*read != '\\' || read + 1 == end) {
            *write++ = *read++;
            continue;
        }
        json_doc_impl::json_descape_conv(read, write, end);
    }

    _dst.resize(write - _dst.data());
}
