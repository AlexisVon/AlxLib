/*****************************************************************/ /**
 * \file   axml.cpp
 * \brief  XML document parsing and manipulation
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "axml.h"

#include "astring.h"

using namespace alx;

std::list<xml_value*> xml_value::content::get_elem(const std::string& _name) {
    std::list<xml_value*> result;
    for (content_meta& i : v_)
        if (i.is_type(ELEM) && i.elem_->name() == _name) result.push_back(i.elem_);
    return result;
}

std::list<const xml_value*> xml_value::content::get_elem(const std::string& _name) const {
    std::list<const xml_value*> result;
    for (const content_meta& i : v_)
        if (i.is_type(ELEM) && i.elem_->name() == _name) result.push_back(i.elem_);
    return result;
}

xml_value::xml_value(const std::string& _name)
    : name_(_name) {
}

xml_value::~xml_value() {
}

json_value alx::xml_value::to_json() const {
    if (attr_.v_.empty() && cont_.v_.size() == 1 && cont_.v_.front().is_type(TEXT))
        return *cont_.v_.front().stri_;

    json_object result;
    for (const auto& attr_pair : attr_.v_)
        result.insert(strutil::format("@_%1", attr_pair.first), attr_pair.second);

    std::map<std::string, std::vector<const content_meta*>> mapcont;
    for (const content_meta& item : cont_.v_)
        switch (item.type()) {
        case TEXT: {
            mapcont["#text"].push_back(&item);
            break;
        }
        case ENTY: {
            mapcont["#enty"].push_back(&item);
            break;
        }
        case ELEM: {
            mapcont[item.elem_->name_].push_back(&item);
            break;
        }
        case NOTE: {
            mapcont["#note"].push_back(&item);
            break;
        }
        case PI__: {
            mapcont["#pi__"].push_back(&item);
            break;
        }
        case CDAT: {
            mapcont["#cdat"].push_back(&item);
            break;
        }
        default: break;
        }
    for (auto map : mapcont) {
        if (map.second.size() == 1)
            result.insert(map.first, map.second.front()->is_type(ELEM) ? map.second.front()->elem_->to_json() : json_value(*map.second.front()->stri_));
        else {
            json_array array;
            for (auto item : map.second)
                array.append(item->is_type(ELEM) ? item->elem_->to_json() : json_value(*item->stri_));
            result.insert(map.first, array);
        }
    }
    return result;
}

bytes xml_value::to_bytes(bool _compact) const {
    return to_bytes_impl(_compact ? max_uint_64 : 0);
}

bytes alx::xml_value::to_bytes_impl(uint_64 _indent_level) const {
    std::string attrs;
    for (const auto& attr_pair : attr_.v_) attrs.append(strutil::format(" %1=\"%2\"", attr_pair.first, escape(attr_pair.second)));

    if (_indent_level != max_uint_64) {
        const std::string indent((uint_64) _indent_level << 1, ' ');
        if (cont_.empty()) return bytes(strutil::format("%1<%2%3/>\n", indent, name_, attrs));
        if (cont_.size() == 1) {
            const auto& item = cont_.v_.front();

            return bytes(strutil::format("%1<%2%3>%4</%2>\n", indent, name_, attrs,
                                         item.is_type(content_type::TEXT) ? escape(*item.stri_) : item.is_type(content_type::ENTY) ? strutil::format("&%1;", *item.stri_)
                                                                                              : item.is_type(content_type::ELEM)   ? strutil::format("\n%1%2", item.elem_->to_bytes_impl(_indent_level + 1).to_string(), indent)
                                                                                              : item.is_type(content_type::NOTE)   ? strutil::format("<!--%1-->", *item.stri_)
                                                                                              : item.is_type(content_type::PI__)   ? strutil::format("<?%1?>", *item.stri_)
                                                                                              : item.is_type(content_type::CDAT)   ? strutil::format("<![CDATA[%1]]>", *item.stri_)
                                                                                                                                   : std::string()));
        } else {
            const std::string indent_1((uint_64) (_indent_level + 1) << 1, ' ');
            bytes result;
            result.append(strutil::format("%1<%2%3>\n", indent, name_, attrs));
            for (auto iter = cont_.v_.cbegin(); iter != cont_.v_.cend(); iter++) {
                // Adjacent text and entity entries are one run on one line; iter is left on the last entry it consumed
                if (iter->is_type(content_type::TEXT) || iter->is_type(content_type::ENTY)) {
                    result.append(indent_1);
                    do {
                        if (iter->is_type(content_type::TEXT)) result.append(escape(*iter->stri_));
                        else if (iter->is_type(content_type::ENTY)) result.append(strutil::format("&%1;", *iter->stri_));
                        else {
                            --iter;
                            break;
                        }
                        if (++iter == cont_.v_.cend()) {
                            --iter;
                            break;
                        }
                    } while (true);
                    result.append_ordinary('\n');
                } else if (iter->is_type(content_type::ELEM)) result.append(iter->elem_->to_bytes_impl(_indent_level + 1));
                else if (iter->is_type(content_type::NOTE)) result.append(strutil::format("%1<!--%2-->\n", indent_1, *iter->stri_));
                else if (iter->is_type(content_type::PI__)) result.append(strutil::format("%1<?%2?>\n", indent_1, *iter->stri_));
                else if (iter->is_type(content_type::CDAT)) result.append(strutil::format("%1<![CDATA[%2]]>\n", indent_1, *iter->stri_));
            }

            result.append(strutil::format("%1</%2>\n", indent, name_));
            return result;
        }
    } else {
        if (cont_.empty()) return bytes(strutil::format("<%1%2/>", name_, attrs));
        bytes result;
        result.append(strutil::format("<%1%2>", name_, attrs));
        for (const content_meta& item : cont_.v_) {
            if (item.is_type(content_type::TEXT)) result.append(escape(*item.stri_));
            else if (item.is_type(content_type::ENTY)) result.append(strutil::format("&%1;", *item.stri_));
            else if (item.is_type(content_type::ELEM)) result.append(item.elem_->to_bytes_impl(_indent_level));
            else if (item.is_type(content_type::NOTE)) result.append(strutil::format("<!--%1-->", *item.stri_));
            else if (item.is_type(content_type::PI__)) result.append(strutil::format("<?%1?>", *item.stri_));
            else if (item.is_type(content_type::CDAT)) result.append(strutil::format("<![CDATA[%1]]>", *item.stri_));
        }

        result.append(strutil::format("</%1>", name_));
        return result;
    }
}

void alx::xml_value::skip_blanks(std::string& _str) {
    if (_str.empty()) return;
    uint_64 head{0}, tail{_str.size()};
    for (; head < tail; head++)
        if (!isspace(_str[head])) break;
    for (; tail != head; tail--)
        if (!isspace(_str[tail - 1])) break;
    if (tail == head) _str.clear();
    else _str = _str.substr(head, tail - head);
}

std::string alx::xml_value::read_name(const bytes_view& _stream, const uint_64 _from, uint_64& _next) {
    if (_from >= _stream.size()) return std::string();
    _next = _from;
    char ch = _stream[_next++];
    // Nothing name-shaped here: _next is restored, which is what lets a caller tell "no name" from "a name that broke"
    if (!std::isalpha(ch) && '_' != ch && ':' != ch) return _next = _from, std::string();
    while (_next < _stream.size()) {
        ch = _stream[_next++];
        if (std::isalnum(ch) || '_' == ch || '-' == ch || '.' == ch || ':' == ch) continue;
        else if (std::isspace(ch) || ch == '>' || ch == '/' || ch == '=') return _next--, std::string((const char*) _stream.data() + _from, _next - _from);
        else return std::string();
    }
    return std::string();
}

bool alx::xml_value::from_bytes_stri(content_meta& _meta, const bytes_view& _stream, const uint_64 _from, uint_64& _next, bool _skip_blanks) {
    if (_stream[_from] != '<' || _from + 2 >= _stream.size()) return false;
    switch (_stream[_from + 1]) {
    case '?': {
        _next = _stream.find("?>", 2, _from + 2);
        if (uint_64_npos == _next) return false;
        else {
            _meta.set_stri(std::string((const char*) _stream.data() + _from + 2, _next - _from - 2), content_type::PI__);
            return _next = _stream.find_ordinary('<', _next + 2), true;
        }
    } break;
    case '!': {
        if (_stream[_from + 2] == '-') {
            if (_from + 6 >= _stream.size()) return false;
            if (_stream[_from + 3] != '-') return false;
            _next = _stream.find("-->", 3, _from + 4);
            if (uint_64_npos == _next) return false;
            else {
                std::string note((const char*) _stream.data() + _from + 4, _next - _from - 4);
                if (_skip_blanks) skip_blanks(note);
                _meta.set_stri(note, content_type::NOTE);
                return _next = _stream.find_ordinary('<', _next + 3), true;
            }
        } else if (_stream[_from + 2] == '[') {
            if (_from + 11 >= _stream.size()) return false;
            if (!_stream.equa("CDATA[", 6, _from + 3)) return false;
            _next = _stream.find("]]>", 3, _from + 9);
            if (uint_64_npos == _next) return false;
            else {
                _meta.set_stri(std::string((const char*) _stream.data() + _from + 9, _next - _from - 9), content_type::CDAT);
                return _next = _stream.find_ordinary('<', _next + 3), true;
            }
        } else if (_stream[_from + 2] == 'D') {
            if (!_stream.equa("DOCTYPE", 7, _from + 2)) return false;
            _next = _stream.find_ordinary('>', _from + 9);
            if (uint_64_npos == _next) return false;
            _meta.set_stri(std::string((const char*) _stream.data() + _from + 2, _next - _from - 2), content_type::DTD_);
            return _next = _stream.find_ordinary('<', _next + 1), true;
        } else return false;
    } break;
    default: return false;
    }
}

bool alx::xml_value::from_bytes_elem(xml_value& _xml, const bytes_view& _stream, const uint_64 _from, uint_64& _next, bool _skip_blanks) {
    if (_from + 4 > _stream.size()) return false;
    _xml.name() = read_name(_stream, _from + 1, _next);
    if (_xml.name().empty()) return false;

    if (_next + 2 > _stream.size()) return false;
    if (_stream[_next] == '/') {
        if (_stream[_next + 1] == '>') return _next = _stream.find_ordinary('<', _next + 2), true;
        else return false;
    } else {
        while (_next < _stream.size() && std::isspace(_stream[_next])) _next++;
        if (_next >= _stream.size()) return false;
        if (_stream[_next] != '>') {
            do {
                const uint_64 attr_from = _next;
                if (!from_bytes_attr(_xml, _stream, _next, _next)) {
                    // _next unmoved means no attribute here at all (the '/' of "<a />"); moved means a broken one
                    if (_next != attr_from) return false;
                    break;
                }
                while (_next < _stream.size() && std::isspace(_stream[_next])) _next++;
                if (_next >= _stream.size()) return false;
            } while (_stream[_next] != '>' && _stream[_next] != '/');
        }

        while (_next < _stream.size() && std::isspace(_stream[_next])) _next++;
        if (_next + 1 >= _stream.size()) return false;
        if (_stream[_next] == '/' && _stream[_next + 1] == '>') {
            return _next = _stream.find_ordinary('<', _next + 2), true;
        }

        if (!from_bytes_cont(_xml, _stream, _next + 1, _next, _skip_blanks)) return false;
        if (_next + 2 + _xml.name().length() >= _stream.size()) return false;
        else {
            if (_stream[_next] == '<' && _stream[_next + 1] == '/' &&
                _stream[_next + 2 + _xml.name().length()] == '>' &&
                _stream.equa(_xml.name().data(), _xml.name().length(), _next + 2))
                return _next = _stream.find_ordinary('<', _next + 3 + _xml.name().length()), true;
            else return false;
        }
    }
}

bool alx::xml_value::from_bytes_attr(xml_value& _xml, const bytes_view& _stream, const uint_64 _from, uint_64& _next) {
    const std::string key = read_name(_stream, _from, _next);
    if (key.empty()) return false;
    if (_xml.attr().contain(key)) return false;

    const uint_64 equ = _stream.find_ordinary('=', _next);
    if (uint_64_npos == equ) return false;
    uint_64 next = equ + 1;
    while (next < _stream.size() && std::isspace(_stream[next])) next++;
    if (next + 4 >= _stream.size()) return false;

    // -1 for an unquoted value becomes uint_64_npos, so it fails exactly like an unterminated one
    const uint_64 end{
        _stream[next] == '\'' ? _stream.find_ordinary('\'', next + 1) : _stream[next] == '\"' ? _stream.find_ordinary('\"', next + 1)
                                                                                              : -1};
    if (uint_64_npos == end) return false;
    else {
        _xml.attr().insert(key, std::string((const char*) _stream.data() + next + 1, end - next - 1));
        _next = end + 1;
        return true;
    }
}

bool alx::xml_value::from_bytes_cont(xml_value& _xml, const bytes_view& _stream, const uint_64 _from, uint_64& _next, bool _skip_blanks) {
    const static std::unordered_map<std::string, char> ref_to_ch{{"lt", '<'}, {"gt", '>'}, {"amp", '&'}, {"quot", '\"'}, {"apos", '\''}};
    uint_64 _begin = _from;
    _next = _stream.find_ordinary('<', _begin);
    if (uint_64_npos == _next || _next + 3 > _stream.size()) return false;
    while (true) {
        if (_next > _from) {
            // idx_end is the last offset consumed, so _begin - 1 stands for "nothing consumed yet"
            uint_64 idx_begin = _stream.find_ordinary('&', _begin, _next), idx_end{_begin - 1};
            std::string text_value;
            while (idx_begin != uint_64_npos) {
                text_value += _stream.mid_view(idx_end + 1, idx_begin - idx_end - 1).to_string();

                idx_end = _stream.find_ordinary(';', idx_begin, _next);
                if (uint_64_npos == idx_end) return false;
                std::string ref_key = _stream.mid_view(idx_begin + 1, idx_end - idx_begin - 1).to_string();
                char ref_ch = alx::map_value(ref_to_ch, ref_key, '\0');
                if (ref_ch != '\0') text_value.push_back(ref_ch);
                else {
                    if (_skip_blanks) skip_blanks(text_value);
                    if (!text_value.empty()) _xml.cont().add_text(text_value);
                    _xml.cont().add_enty(ref_key);
                    text_value.clear();
                }
                idx_begin = _stream.find_ordinary('&', idx_end + 1, _next);
            }
            text_value += _stream.mid_view(idx_end + 1, _next - idx_end - 1).to_string();
            if (_skip_blanks) skip_blanks(text_value);
            if (!text_value.empty()) _xml.cont().add_text(text_value);
        }
        if (_stream[_next + 1] != '/') {
            content_meta meta;
            // Both parsers take (stream, _from, _next): the position goes in as _next here and comes back in _begin
            if (from_bytes_stri(meta, _stream, _next, _begin, _skip_blanks)) _xml.cont().add_meta(std::move(meta));
            else if (from_bytes_elem(*meta.set_elem(""), _stream, _next, _begin, _skip_blanks)) _xml.cont().add_meta(std::move(meta));
            else return false;
            _next = _stream.find_ordinary('<', _begin);
            if (uint_64_npos == _next || _next + 3 > _stream.size()) return false;
        } else return true;
    }
}

std::string xml_value::escape(const std::string& _str) {
    if (_str.empty()) return _str;
    size_t escaped_size = _str.length();
    for (char c : _str) {
        switch (c) {
        case '&': escaped_size += 4; break;
        case '<': escaped_size += 3; break;
        case '>': escaped_size += 3; break;
        case '"': escaped_size += 5; break;
        case '\'': escaped_size += 5; break;
        default: break;
        }
    }
    std::string result;
    result.reserve(escaped_size);
    for (char c : _str) {
        switch (c) {
        case '&': result.append("&amp;"); break;
        case '<': result.append("&lt;"); break;
        case '>': result.append("&gt;"); break;
        case '"': result.append("&quot;"); break;
        case '\'': result.append("&apos;"); break;
        default: result.append(1, c); break;
        }
    }
    return result;
}

xml_object::xml_object(const std::string& _name)
    : xml_value(_name) {
}

xml_object::~xml_object() {
}

bytes xml_object::to_bytes(bool _compact) const {
    bytes result;
    for (const content_meta& item : gcont_.container()) {
        if (item.is_type(content_type::PI__)) result.append(strutil::format("<?%1?>%2", *item.stri_, _compact ? "" : "\n"));
        else if (item.is_type(content_type::DTD_)) result.append(strutil::format("<!%1>%2", *item.stri_, _compact ? "" : "\n"));
        else if (item.is_type(content_type::NOTE)) result.append(strutil::format("<!--%1-->%2", *item.stri_, _compact ? "" : "\n"));
    }
    result.append(xml_value::to_bytes(_compact));
    return result;
}

json_object alx::xml_object::to_json() const {
    json_object result;

    std::map<std::string, std::vector<const content_meta*>> mapcont;
    for (const content_meta& item : gcont_.v_)
        switch (item.type()) {
        case PI__: {
            mapcont["#pi__"].push_back(&item);
            break;
        }
        case DTD_: {
            mapcont["#dtd_"].push_back(&item);
            break;
        }
        case NOTE: {
            mapcont["#note"].push_back(&item);
            break;
        }
        default: break;
        }
    for (auto map : mapcont) {
        if (map.second.size() == 1)
            result.insert(map.first, json_value(*map.second.front()->stri_));
        else {
            json_array array;
            for (auto item : map.second) array.append(json_value(*item->stri_));
            result.insert(map.first, array);
        }
    }
    result.insert(name_, xml_value::to_json());
    return result;
}

xml_object alx::xml_object::from_bytes(const bytes_view& _xml, bool _skip_blanks, bool* _ok) {
    bool t_ok{false};
    bool& success = nullptr == _ok ? t_ok : *_ok;
    uint_64 index{_xml.find_ordinary('<', 0)};
    if (uint_64_npos == index) return success = false, xml_object();

    xml_object result;
    content_meta meta;
    // index goes in as _from and comes back as _next; _from is taken by value, so the same variable twice is safe
    while (uint_64_npos != index && xml_value::from_bytes_stri(meta, _xml, index, index, _skip_blanks))
        result.gcont_.add_meta(std::move(meta));

    if (uint_64_npos == index) return success = false, xml_object();
    // Complete only if the root element ate the rest: a later '<' leaves index short of npos and fails the parse
    else return xml_value::from_bytes_elem(result, _xml, index, index, _skip_blanks) && uint_64_npos == index ? (success = true, std::move(result)) : (success = false, xml_object());
}
