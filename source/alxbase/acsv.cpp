/*****************************************************************/ /**
 * \file   acsv.cpp
 * \brief  CSV text matrix (row 0 is the header row)
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "acsv.h"
#include "astring.h"

using namespace alx;

namespace {

    inline bool valid_delim(char _delim) {
        const uint_8 ch = (uint_8) _delim;
        return ch > 0 && ch < 0x80 && '"' != ch && '\n' != ch && '\r' != ch;
    }

    // ST_TAIL: the field was closed by a quote -- only another quote, a delimiter, or an end of line may follow
    enum cell_state { ST_HEAD,
                      ST_FIELD,
                      ST_QUOTED,
                      ST_TAIL };

    inline bool has_high_byte(const std::string& _cell) {
        for (char ch : _cell)
            if ((uint_8) ch >= 0x80) return true;
        return false;
    }

    // the structural bytes are ASCII in every encoding accepted here, so only a cell may need transcoding
    bool parse_bytes(const char* _data, uint_64 _size, char _delim, bool _strict, strutil::CODE_FORMAT _cell_format,
                     varvec& _rows) {
        varvec rows;
        varvec row;
        std::string cell;
        uint_64 header_fields = 0;
        bool row_open = false;
        cell_state state = ST_HEAD;

        auto end_field = [&]() -> bool {
            if (strutil::UTF8 != _cell_format && has_high_byte(cell)) {
                std::string converted = strutil::code_conver(cell.data(), cell.size(), strutil::UTF8, _cell_format);
                if (converted.empty()) return false;
                cell = std::move(converted);
            }
            row.push_back(std::move(cell));
            cell.clear();
            row_open = true;
            state = ST_HEAD;
            return true;
        };
        auto end_row = [&]() -> bool {
            if (!end_field()) return false;
            if (_strict) {
                if (rows.empty()) header_fields = row.size();
                else if (row.size() != header_fields) return false;
            }
            rows.push_back(std::move(row));
            row = varvec();
            row_open = false;
            return true;
        };

        for (uint_64 i = 0; i < _size; i++) {
            const char ch = _data[i];
            const bool eol = ('\n' == ch || '\r' == ch);

            if (ST_QUOTED == state) {
                if ('"' == ch) state = ST_TAIL;
                else cell.push_back(ch);
                continue;
            }
            if (ST_TAIL == state && '"' == ch) {
                cell.push_back('"');
                state = ST_QUOTED;
                continue;
            }
            if (ST_TAIL == state && !eol && ch != _delim) return false;
            if (eol) {
                if ('\r' == ch && i + 1 < _size && '\n' == _data[i + 1]) i++;
                if (!row_open) continue;
                if (!end_row()) return false;
                continue;
            }
            if (ST_HEAD == state) {
                if ('"' == ch) {
                    state = ST_QUOTED;
                    row_open = true;
                    continue;
                }
                if (ch == _delim) {
                    if (!end_field()) return false;
                    continue;
                }
                state = ST_FIELD;
            }
            if (ch == _delim) {
                if (!end_field()) return false;
                continue;
            }
            if ('"' == ch) return false;
            cell.push_back(ch);
            row_open = true;
        }

        if (ST_QUOTED == state) return false;
        if (row_open && !end_row()) return false;
        _rows = std::move(rows);
        return true;
    }

    // a first or last byte at or below a space is quoted: a reader that trims would otherwise eat it
    bool needs_quote(const std::string& _cell, char _delim) {
        if (_cell.empty()) return false;
        if ((uint_8) _cell.front() <= ' ' || (uint_8) _cell.back() <= ' ') return true;
        for (char ch : _cell)
            if (ch == _delim || ch == '"' || ch == '\n' || ch == '\r') return true;
        return false;
    }

    bool write_cell(const std::string& _cell, ostream& _out, char _delim, bool _only_cell) {
        if (_cell.empty()) return _only_cell ? _out.append("\"\"", 2) : true;
        if (!needs_quote(_cell, _delim)) return _out.append(_cell.data(), _cell.size());

        std::string quoted;
        quoted.reserve(_cell.size() + 2);
        quoted.push_back('"');
        for (char ch : _cell) {
            if ('"' == ch) quoted.push_back('"');
            quoted.push_back(ch);
        }
        quoted.push_back('"');
        return _out.append(quoted.data(), quoted.size());
    }

    bool write_row(const varvec& _row, ostream& _out, char _delim) {
        // a bare newline would read back as a blank line, which is no record at all
        if (_row.empty()) return _out.append("\"\"\n", 3);
        for (size_t c = 0; c < _row.size(); c++) {
            if (!_row[c].is<std::string>()) return false;
            if (c > 0 && !_out.append(&_delim, 1)) return false;
            if (!write_cell(_row[c].to<std::string>(), _out, _delim, 1 == _row.size())) return false;
        }
        return _out.append("\n", 1);
    }

    bool write_rows(const varvec& _rows, ostream& _out, char _delim) {
        for (const variant& r : _rows) {
            if (!r.is<varvec>()) return false;
            if (!write_row(r.to<varvec>(), _out, _delim)) return false;
        }
        return true;
    }

    bool write_rows(varvec& _rows, ostream& _out, char _delim) {
        for (variant& r : _rows) {
            if (!r.is<varvec>()) return false;
            if (!write_row(r.to<varvec>(), _out, _delim)) return false;
            r = varvec();
        }
        _rows.clear();
        return true;
    }
}

varvec csv::from_bytes(const char* _data, uint_64 _size, char _delim, bool _strict, bool* _ok) {
    varvec result;
    bool ok = nullptr != _data && valid_delim(_delim);

    if (ok && _size > 0) {
        const char* text = _data;
        uint_64 text_size = _size;
        std::string converted;
        strutil::CODE_FORMAT cell_format = strutil::UTF8;

        bool ascii = true;
        for (uint_64 i = 0; i < _size; i++)
            if ((uint_8) _data[i] >= 0x80) {
                ascii = false;
                break;
            }

        if (!ascii) {
            const strutil::CODE_FORMAT fmt = strutil::detect_format(_data, _size);
            if ((fmt & strutil::ENCODE_UTF16) != 0) {

                // two bytes per character: the delimiter is not one ASCII byte here, so the whole text has to be converted first
                converted = strutil::code_conver(_data, _size, strutil::UTF8, fmt);
                if (!converted.empty()) {
                    text = converted.data();
                    text_size = converted.size();
                } else if ((fmt & strutil::PROPERTY_BOM) != 0 && _size <= 2U) {
                    // a lone BOM converts to nothing, which is an empty matrix, not a failure
                    text = "";
                    text_size = 0;
                } else {
                    ok = false;
                }
            } else if (strutil::UTF8_BOM == fmt) {
                text = _data + 3;
                text_size = _size - 3;
            } else if (strutil::UTF8 != fmt) {
                cell_format = fmt;
            }
        }
        if (ok) ok = parse_bytes(text, text_size, _delim, _strict, cell_format, result);
    }

    if (!ok) result.clear();
    if (_ok) *_ok = ok;
    return result;
}

bool csv::to_bytes(const varvec& _rows, ostream& _out, char _delim) {
    if (!valid_delim(_delim)) return false;
    return write_rows(_rows, _out, _delim);
}

bool csv::to_bytes(varvec&& _rows, ostream& _out, char _delim) {
    if (!valid_delim(_delim)) return false;
    return write_rows(_rows, _out, _delim);
}

bytes csv::to_bytes(const varvec& _rows, char _delim) {
    if (!valid_delim(_delim)) return bytes();
    bytes result;
    ostream_buff out(result);
    if (!write_rows(_rows, out, _delim)) return bytes();
    // null is the failure sentinel, so an empty table has to come back as a non-null buffer
    if (result.null()) result = bytes((uint_64) 0);
    return result;
}

bytes csv::to_bytes(varvec&& _rows, char _delim) {
    if (!valid_delim(_delim)) return bytes();
    bytes result;
    ostream_buff out(result);
    if (!write_rows(_rows, out, _delim)) return bytes();
    // null is the failure sentinel, so an empty table has to come back as a non-null buffer
    if (result.null()) result = bytes((uint_64) 0);
    return result;
}
