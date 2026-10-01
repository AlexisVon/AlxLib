/*****************************************************************/ /**
 * \file   ahttp.cpp
 * \brief  HTTP protocol client/server implementation
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "ahttp.h"
#include "acompress.h"
#include "adatetime.h"
#include "astring.h"
#include "atransmit.h"
#include "autility.h"

using namespace alx;
using namespace alx::http;

namespace {
    const std::unordered_map<std::string, version> version_string2key{
        {"HTTP/1.0", version::V1_0},
        {"HTTP/1.1", version::V1_1},
    };
    const std::unordered_map<version, std::string> version_key2string{
        {version::V1_0, "HTTP/1.0"},
        {version::V1_1, "HTTP/1.1"},
    };
    const std::unordered_map<std::string, action> action_string2key{
        {"GET", action::GET},
        {"HEAD", action::HEAD},
        {"POST", action::POST},
        {"PUT", action::PUT},
        {"DELETE", action::DELETE},
        {"CONNECT", action::CONNECT},
        {"OPTIONS", action::OPTIONS},
        {"TRACE", action::TRACE},
        {"PATCH", action::PATCH},
    };
    const std::unordered_map<action, std::string> action_key2string{
        {action::GET, "GET"},
        {action::HEAD, "HEAD"},
        {action::POST, "POST"},
        {action::PUT, "PUT"},
        {action::DELETE, "DELETE"},
        {action::CONNECT, "CONNECT"},
        {action::OPTIONS, "OPTIONS"},
        {action::TRACE, "TRACE"},
        {action::PATCH, "PATCH"},
    };
    const std::unordered_map<uint_16, std::string> state_key2string{

        {state::info::CONTINUE, "100 Continue"},
        {state::info::SWITCHING_PROTOCOLS, "101 Switching Protocols"},
        {state::info::PROCESSING, "102 Processing"},
        {state::info::EARLY_HINT, "103 Early Hints"},

        {state::succ::OK, "200 OK"},
        {state::succ::CREATED, "201 Created"},
        {state::succ::ACCEPTED, "202 Accepted"},
        {state::succ::NO_AUTH_INFO, "203 Non-Authoritative Information"},
        {state::succ::NO_CONTENT, "204 No Content"},
        {state::succ::RESET_CONTENT, "205 Reset Content"},
        {state::succ::PARTIAL_CONTENT, "206 Partial Content"},
        {state::succ::MULTI_STATUS, "207 Multi-Status"},
        {state::succ::ALREADY_REPORTED, "208 Already Reported"},
        {state::succ::IM_USED, "226 IM Used"},

        {state::rurl::MOVED_PERMANENTLY, "301 Moved Permanently"},
        {state::rurl::FOUND, "302 Found"},
        {state::rurl::SEE_OTHER, "303 See Other"},
        {state::rurl::NOT_MODIFIED, "304 Not Modified"},
        {state::rurl::USE_PROXY, "305 Use Proxy"},
        {state::rurl::TEMPORARY_REDIRECT, "307 Temporary Redirect"},
        {state::rurl::PERMANENT_REDIRECT, "308 Permanent Redirect"},

        {state::cerr::BAD_REQUEST, "400 Bad Request"},
        {state::cerr::UNAUTHORIZED, "401 Unauthorized"},
        {state::cerr::FORBIDDEN, "403 Forbidden"},
        {state::cerr::NOT_FOUND, "404 Not Found"},
        {state::cerr::METHOD_NOT_ALLOWED, "405 Method Not Allowed"},
        {state::cerr::NOT_ACCEPTABLE, "406 Not Acceptable"},
        {state::cerr::PROXY_AUTH_REQUIRED, "407 Proxy Authentication Required"},
        {state::cerr::REQUEST_TIMEOUT, "408 Request Timeout"},
        {state::cerr::CONFLICT, "409 Conflict"},
        {state::cerr::GONE, "410 Gone"},
        {state::cerr::LENGTH_REQUIRED, "411 Length Required"},
        {state::cerr::PRECONDITION_FAILED, "412 Precondition Failed"},
        {state::cerr::PAYLOAD_TOO_LARGE, "413 Payload Too Large"},
        {state::cerr::URI_TOO_LONG, "414 URI Too Long"},
        {state::cerr::UNSUPPORTED_MEDIA_TYPE, "415 Unsupported Media Type"},
        {state::cerr::RANGE_NOT_SATISFIABLE, "416 Range Not Satisfiable"},
        {state::cerr::EXPECTATION_FAILED, "417 Expectation Failed"},
        {state::cerr::IM_A_TEAPOT, "418 I'm a teapot"},
        {state::cerr::MISDIRECTED_REQUEST, "421 Misdirected Request"},
        {state::cerr::UNPROCESSABLE_ENTITY, "422 Unprocessable Entity"},
        {state::cerr::LOCKED, "423 Locked"},
        {state::cerr::FAILED_DEPENDENCY, "424 Failed Dependency"},
        {state::cerr::TOO_EARLY, "425 Too Early"},
        {state::cerr::UPGRADE_REQUIRED, "426 Upgrade Required"},
        {state::cerr::PRECONDITION_REQUIRED, "428 Precondition Required"},
        {state::cerr::TOO_MANY_REQUESTS, "429 Too Many Requests"},
        {state::cerr::REQUEST_HEADER_FIELDS_TOO_LARGE, "431 Request Header Fields Too Large"},
        {state::cerr::UNAVAILABLE_FOR_LEGAL_REASONS, "451 Unavailable For Legal Reasons"},

        {state::serr::INTERNAL_SERVER_ERROR, "500 Internal Server Error"},
        {state::serr::NOT_IMPLEMENTED, "501 Not Implemented"},
        {state::serr::BAD_GATEWAY, "502 Bad Gateway"},
        {state::serr::SERVICE_UNAVAILABLE, "503 Service Unavailable"},
        {state::serr::GATEWAY_TIMEOUT, "504 Gateway Timeout"},
        {state::serr::HTTP_VERSION_NOT_SUPPORTED, "505 HTTP Version Not Supported"},
        {state::serr::VARIANT_ALSO_NEGOTIATES, "506 Variant Also Negotiates"},
        {state::serr::INSUFICIENT_STORAGE, "507 Insufficient Storage"},
        {state::serr::LOOP_DETECTED, "508 Loop Detected"},
        {state::serr::NOT_EXTENDED, "510 Not Extended"},
        {state::serr::NETWORK_AUTH_REQUIRED, "511 Network Authentication Required"}};
    const std::unordered_map<uint_16, std::string> mime_key2string{

        {mime::txt::plain, "text/plain"},
        {mime::txt::html, "text/html"},
        {mime::txt::css, "text/css"},
        {mime::txt::csv, "text/csv"},

        {mime::app::json, "application/json"},
        {mime::app::xml, "application/xml"},
        {mime::app::js, "application/javascript"},
        {mime::app::pdf, "application/pdf"},
        {mime::app::zip, "application/zip"},
        {mime::app::gzip, "application/gzip"},
        {mime::app::tar, "application/x-tar"},
        {mime::app::oct, "application/octet-stream"},

        {mime::img::webp, "image/webp"},
        {mime::img::bmp, "image/bmp"},
        {mime::img::png, "image/png"},
        {mime::img::jpeg, "image/jpeg"},
        {mime::img::gif, "image/gif"},
        {mime::img::ico, "image/vnd.microsoft.icon"},
        {mime::img::svg, "image/svg+xml"},

        {mime::aud::mpeg, "audio/mpeg"},
        {mime::aud::wav, "audio/wav"},
        {mime::aud::ogg, "audio/ogg"},

        {mime::vid::webm, "video/webm"},
        {mime::vid::mp4, "video/mp4"},
        {mime::vid::avi, "video/x-msvideo"},

        {mime::fnt::woff, "font/woff"},
        {mime::fnt::woff2, "font/woff2"}};

    // Cap on a size a peer declares: bounds the request body and one decoded chunk alike
    constexpr uint_64 max_body_size{64 * 1024 * 1024};
    bool parse_uint(const std::string& _str, const uint_64 _max, uint_64& _value) {
        if (_str.empty()) return false;
        uint_64 value{0};
        for (char c : _str) {
            if (c < '0' || c > '9') return false;
            const uint_64 digit = uint_64(c - '0');
            if (value > _max / 10 || (value == _max / 10 && digit > _max % 10)) return false;
            value = value * 10 + digit;
        }
        _value = value;
        return true;
    }

    // false with _need_more set means "wait for more bytes"; false without it means malformed
    bool decode_chunked(const bytes_view& _in, bytes& _out, bool& _need_more) {
        _out.clear();
        _need_more = false;
        uint_64 pos{0};
        while (true) {
            const uint_64 eol = _in.find("\r\n", 2, pos);
            if (uint_64_npos == eol) {
                _need_more = true;
                return false;
            }
            // 16 is all the digits a uint_64 chunk size can use, and it is the bound that keeps the shift below from overflowing
            if (eol == pos || eol - pos > 16) return false;

            uint_64 size{0};
            for (uint_64 i = pos; i < eol; ++i) {
                const uint_8 c = _in.data()[i];
                uint_8 value{0};
                if (c >= '0' && c <= '9') value = c - '0';
                else if (c >= 'a' && c <= 'f') value = c - 'a' + 10;
                else if (c >= 'A' && c <= 'F') value = c - 'A' + 10;
                else return false;
                size = (size << 4) | value;
            }
            // zero size is the terminating chunk; trailers behind it are not consumed
            if (0 == size) return true;
            if (size > max_body_size) return false;

            const uint_64 data_begin = eol + 2;
            const uint_64 next = data_begin + size;
            if (_in.size() < next + 2) {
                _need_more = true;
                return false;
            }
            if (_in.data()[next] != '\r' || _in.data()[next + 1] != '\n') return false;
            _out.append(_in.data() + data_begin, size);
            pos = next + 2;
        }
    }
    std::pair<uint_64, uint_64> find_request_head(const bytes_view& _data) {
        uint_64 start = _data.find("HTTP/1.", 7, 0);
        if (start == uint_64_npos) return {uint_64_npos, uint_64_npos};
        // The walk back goes version token -> space ending the URL -> space ending the method
        // -> first char of the line; both guards reject a line too short for "METHOD SP URL SP"
        if (start < 6) return {uint_64_npos, uint_64_npos};
        start -= 2;
        while (start >= 3 && _data[--start] != ' ');
        if (start < 3) return {uint_64_npos, uint_64_npos};
        while (start > 0 && _data[start - 1] != '\n') --start;

        // from start, not 0: an earlier blank line would make the range end before it begins
        return {start, _data.find("\r\n\r\n", 4, start)};
    }
    // No walk back here: a status line opens with the version token, so the match is the head start
    std::pair<uint_64, uint_64> find_reply_head(const bytes_view& _data) {
        uint_64 start = _data.find("HTTP/1.", 7, 0);
        uint_64 end = start + 7;
        if (start == uint_64_npos) return {uint_64_npos, uint_64_npos};
        end = _data.find("\r\n\r\n", 4, start);
        return {start, end};
    }
    std::string encode_string(const std::string& _str) {
        static const char hex_upper[]{"0123456789ABCDEF"};
        std::string result;
        result.reserve(_str.size() << 1);
        for (char c : _str) {
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                c == '-' || c == '_' || c == '.' || c == '~') result += c;
            else {
                result += '%';
                result += hex_upper[(uint_8) c >> 4];
                result += hex_upper[(uint_8) c & 0X0F];
            }
        }
        return result;
    }
    std::string decode_string(const std::string& _str) {
        std::string result;
        result.reserve(_str.length());
        for (size_t i = 0; i < _str.length(); ++i) {
            if (_str[i] == '%' && i + 2 < _str.length()) {
                const bytes hex = bytes::from_hex(_str.substr(i + 1, 2));
                if (hex.empty()) result.push_back(_str[i]);
                else {
                    result.push_back(static_cast<char>(hex.data()[0]));
                    i += 2;
                }
            } else if (_str[i] == '+') result.push_back(' ');
            else result.push_back(_str[i]);
        }
        return result;
    }
    void encode_url(
        const std::list<std::string>& _urlwords,
        const std::map<std::string, std::string>& _param,
        std::string& _urlorg) {
        std::list<std::string> temp;
        for (auto& word : _urlwords) temp.push_back(encode_string(word));
        _urlorg = "/" + strutil::join(temp, "/");

        if (!_param.empty()) {
            _urlorg += "?";
            for (const auto& it : _param)
                _urlorg += strutil::format("%1=%2&", encode_string(it.first), encode_string(it.second));
            _urlorg.pop_back();
        }
    }
    void decode_url(
        const std::string& _urlorg,
        std::list<std::string>& _urlwords,
        std::map<std::string, std::string>& _param) {
        std::list<std::string> lurl = strutil::split(_urlorg, "?");
        if (lurl.size() > 0) {
            _urlwords = strutil::split(lurl.front(), "/");
            _urlwords.remove_if([](const std::string& _str) -> bool { return _str.empty(); });
            for (std::string& word : _urlwords) word = decode_string(word);
        }
        if (lurl.size() > 1) {
            std::list<std::string> t = strutil::split(lurl.back(), "&");
            for (const auto& it : t) {
                if (it.empty()) continue;
                std::list<std::string> a = strutil::split(it, "=");
                if (a.size() == 2) _param[decode_string(a.front())] = decode_string(a.back());
            }
        };
    }
}

alx::http::request::request(action _action, version _version, bool _alive)
    : m_action(_action), m_version(_version), m_alive(_alive) {
    m_field["Connection"] = _alive ? "keep-alive" : "close";
}

bytes alx::http::request::to_bytes() const {
    bytes result(strutil::format(
        "%1 %2 %3\r\n",
        alx::map_value(action_key2string, m_action),
        m_urlorg,
        alx::map_value(version_key2string, m_version)));

    for (const auto& i : m_field)
        result.append(strutil::format("%1: %2\r\n", i.first, i.second));
    result.append("\r\n");
    if (m_content.size() != 0) result.append(m_content);
    return result;
}

void alx::http::request::set_urlwords(
    const std::list<std::string>& _urlwords,
    const std::map<std::string, std::string>& _param) {
    m_urlwords = _urlwords;
    m_param = _param;
    encode_url(m_urlwords, m_param, m_urlorg);
}

void alx::http::request::set_content(const json_object& _val) {
    set_content("application/json", bytes(json_doc::to_json(_val, true)));
}

void alx::http::request::set_content(uint_16 _mime, const bytes_view& _val) {
    set_content(alx::map_value<uint_16, std::string>(mime_key2string, _mime, "application/octet-stream"), _val);
}

void alx::http::request::set_content(const std::string& _mime, const bytes_view& _val) {
    m_content = _val.to_bytes();
    m_field["Content-Type"] = _mime;
    m_field["Content-Length"] = std::to_string(_val.size());
}

alx::http::reply::reply(version _v, uint_16 _s, bool _alive)
    : m_alive(_alive), m_version(_v), m_state(_s) {
    m_field["Connection"] = _alive ? "keep-alive" : "close";
}

bytes alx::http::reply::to_bytes() const {
    bytes result(strutil::format("%1 %2\r\n",
                                 alx::map_value(version_key2string, m_version),
                                 alx::map_value(state_key2string, m_state)));
    for (const auto& i : m_field)
        result.append(strutil::format("%1: %2\r\n", i.first, i.second));
    result.append("\r\n");
    if (m_content.size() != 0) result.append(m_content);
    return result;
}

void alx::http::reply::set_content(const json_object& _val) {
    set_content("application/json", bytes(json_doc::to_json(_val, true)));
}

void alx::http::reply::set_content(uint_16 _mime, const bytes_view& _val) {
    set_content(alx::map_value<uint_16, std::string>(mime_key2string, _mime, "application/octet-stream"), _val);
}

void alx::http::reply::set_content(const std::string& _mime, const bytes_view& _val) {
    m_content = _val.to_bytes();
    m_field["Content-Type"] = _mime;
    m_field["Content-Length"] = std::to_string(_val.size());
}

reply api::exec(const request& _req, std::list<std::string> _url) {
    if (_url.empty())
        return nullptr != m_func ? m_func(_req) : reply(_req.get_version(), state::FORBIDDEN, _req.m_alive);

    std::string current = _url.front();
    _url.pop_front();

    api* next = alx::map_value(m_api_map, current);
    if (nullptr == next) {
        auto it = m_alias_map.find(current);
        if (it != m_alias_map.end()) next = alx::map_value(m_api_map, it->second);
    }
    if (nullptr != next) return next->exec(_req, _url);
    else return reply(_req.get_version(), state::NOT_FOUND, _req.m_alive);
}

void api::insert(api* _api, const std::string& _word) {
    delete m_api_map[_word];
    m_api_map[_word] = _api;
}

bool api::add_alias(const std::string& _alias, const std::string& _word) {
    if (m_api_map.count(_alias) || m_alias_map.count(_alias)) return false;
    if (!m_api_map.count(_word)) return false;
    m_alias_map[_alias] = _word;
    return true;
}

server::server(transmit* _trans, api* _api, uint_64 _tsize, uint_64 _qsize)
    : comm(_trans), m_api(_api), m_pool(new threadpool(_tsize, _qsize)) {
}

server::server(const std::string _addr_port, api* _api, uint_64 _tsize, uint_64 _qsize)
    : server(transmit::create("tcp_server", _addr_port), _api, _tsize, _qsize) {
}

server::~server() {
    close();
    delete m_pool;
    delete m_api;
}

static transmit* make_tls_transmit(const std::string& _addr, const varmap& _ext) {
    varmap ext = _ext;
    if (!ext.contain("alpn")) ext.insert("alpn", std::string("http/1.1"));
    return transmit::create("tls_server", _addr, ext);
}

server_tls::server_tls(const std::string _addr_port, api* _api, const varmap& _ext_info,
                       uint_64 _tsize, uint_64 _qsize)
    : server(make_tls_transmit(_addr_port, _ext_info), _api, _tsize, _qsize) {
}

void server::on_bytes_recv(const bytes_view& _data, uint_64 _loc) {
    std::list<request> pack_list;
    parse_bytes(_data, pack_list);
    for (const auto& pack : pack_list)
        m_pool->enqueue(std::bind(&server::process_request, this, pack, _loc));
}

void alx::http::server::parse_bytes(const bytes_view& _data, std::list<request>& _pack) {
    static thread_local bytes cache;
    cache.append(_data);
    _pack.clear();

    while (cache.size() != 0) {
        const std::pair<uint_64, uint_64> head_range = find_request_head(cache);
        if (uint_64_npos == head_range.first || head_range.second < head_range.first) {
            mesg_prit.exec("HTTP head not found, drop cache!");
            cache.clear();
            return;
        }
        if (uint_64_npos == head_range.second) {
            if (cache.size() >= 0X2000) {
                mesg_prit.exec("HTTP head too big(>8K), drop cache!");
                cache.clear();
            }
            return;
        }
        uint_64 index = cache.find("Content-Length: ", 16, head_range.first, head_range.second);
        uint_64 content_size{0};
        if (index != uint_64_npos) {
            index += 16;
            if (index >= head_range.second || !std::isdigit(cache.data()[index])) {
                mesg_prit.exec("HTTP head parse \"Content-Length\" failed!");
                cache = cache.mid(head_range.second + 4);
                continue;
            }

            // by hand: the cache is not NUL terminated, std::stoi would read past the end of the head
            while (index < head_range.second && std::isdigit(cache.data()[index])) {
                content_size = content_size * 10 + (cache.data()[index] - '0');
                if (content_size > max_body_size) {
                    mesg_prit.exec("HTTP content too big, drop cache!");
                    cache.clear();
                    return;
                }
                ++index;
            }
        }

        if (cache.size() < head_range.second + 4 + content_size) {
            cache = cache.mid(head_range.first);
            return;
        }

        request temp;
        bool success{true};
        std::list<std::string> head_tag = strutil::split(std::string((const char*) cache.data() + head_range.first, head_range.second - head_range.first), "\r\n");
        for (const std::string& tag : head_tag) {
            if (tag == head_tag.front()) {
                std::list<std::string> keys = strutil::split(tag, " ");
                if (keys.size() == 3) {
                    temp.m_action = alx::map_value(action_string2key, keys.front(), action::NONE);
                    temp.m_urlorg = *(++keys.begin());
                    decode_url(temp.m_urlorg, temp.m_urlwords, temp.m_param);
                    temp.m_version = alx::map_value(version_string2key, keys.back(), version::NONE);
                } else {
                    mesg_prit.exec("HTTP head info error!");
                    success = false;
                }
            } else {
                std::list<std::string> keys = strutil::split(tag, ": ");
                if (keys.size() == 2) temp.m_field[keys.front()] = keys.back();
                else {
                    mesg_prit.exec("HTTP head field error!");
                    success = false;
                }
            }
        }
        if (!success) {
            cache = cache.mid(head_range.second + 4);
            continue;
        }
        temp.m_content = cache.mid(head_range.second + 4, content_size);
        temp.m_alive = alx::map_value(temp.m_field, std::string("Connection")) == "keep-alive";
        _pack.push_back(std::move(temp));
        cache = cache.mid(head_range.second + 4 + content_size);
    }
}

void alx::http::server::process_request(const request& _request, uint_64 _loc) {
    datetime cost;
    reply rpl = m_api->exec(_request, _request.m_urlwords);
    rpl.set_field("Server", "AlexisHttpServer/1.0");

    auto it = _request.m_field.find("Accept-Encoding");
    if (it != _request.m_field.end() && it->second.find("gzip") != std::string::npos && rpl.m_content.size() >= 256) {

        auto ct = rpl.m_field.find("Content-Type");
        bool compressible = true;
        if (ct != rpl.m_field.end()) {
            const std::string& mime = ct->second;
            if (mime.find("image/") == 0 || mime.find("video/") == 0 ||
                mime.find("audio/") == 0 || mime.find("font/") == 0 ||
                mime == "application/gzip" || mime == "application/zip" ||
                mime == "application/x-tar") {
                compressible = false;
            }
        }
        if (compressible) {
            bytes compressed = compress::encoder_gzip::sexec(rpl.m_content);
            if (!compressed.empty()) {
                rpl.m_content = std::move(compressed);
                rpl.m_field["Content-Encoding"] = "gzip";
                rpl.m_field["Content-Length"] = std::to_string(rpl.m_content.size());
            }
        }
    }
    reply_request(rpl, _loc);
    mesg_prit.exec(strutil::format(
        "reply %1 %2 cost: %3ms, \"%4\" \"%5\"",
        transmit::to_strip(_loc),
        alx::map_value(action_key2string, _request.m_action),
        std::to_string(cost.elapsed_ms()),
        alx::map_value(state_key2string, rpl.m_state),
        _request.m_urlorg));
}

void server::reply_request(const reply& _reply, uint_64 _loc) {
    bytes reply(strutil::format(
        "%1 %2\r\n",
        alx::map_value(version_key2string, _reply.m_version),
        alx::map_value(state_key2string, _reply.m_state)));
    for (const auto& i : _reply.m_field)
        reply.append(strutil::format(
            "%1: %2\r\n",
            i.first,
            i.second));
    reply.append("\r\n");
    if (_reply.m_content.size() != 0) reply.append(_reply.m_content);
    bytes_send(reply, _loc);
    if (!_reply.m_alive) close(_loc);
}

client::client(transmit* _trans)
    : comm(_trans) {
}

client::client(const std::string _addr_port)
    : client(transmit::create("tcp_client", _addr_port)) {
}

client_tls::client_tls(const std::string _addr_port, const varmap& _ext_info)
    : client(transmit::create("tls_client", _addr_port, _ext_info)) {
}

client::~client() {
    // close() first: it joins the receiving thread, which would otherwise be mid-parse on m_reply
    close();
    abort_pending();
}

// m_promise crosses between the caller and the transport thread with no lock: one request in flight keeps them apart
bool client::exec(const request& _request, std::future<reply>& _reply) {
    if (!is_connect() || nullptr != m_promise) return false;

    m_promise = new std::promise<reply>();
    _reply = m_promise->get_future();
    request req = _request;
    req.m_field["Accept-Encoding"] = "gzip";
    if (bytes_send(req.to_bytes())) return true;
    else {
        m_promise->set_value(reply());
        safe_delete(m_promise);
        return false;
    }
}

void client::on_bytes_recv(const bytes_view& _data, uint_64 _loc) {
    if (nullptr == m_promise) {
        mesg_prit.exec("No active request, dropping received data!");
        return;
    }
    m_buffer.append(_data);

    // a non-null m_reply means the head is parsed: this call resumes at the body
    if (nullptr == m_reply) {
        const std::pair<uint_64, uint_64> head_range = find_reply_head(m_buffer);
        if (uint_64_npos == head_range.first) {
            mesg_prit.exec("HTTP head not found, drop cache!");
            m_buffer = m_buffer.right(0X1000);
            return;
        }
        if (uint_64_npos == head_range.second) {
            if (m_buffer.size() >= 0X2000) {
                mesg_prit.exec("HTTP head too big(>8K), drop cache!");
                m_buffer.clear();
            }
            return;
        }
        m_reply = new reply();
        bool success{true};
        std::list<std::string> head_tag = strutil::split(std::string((const char*) m_buffer.data() + head_range.first, head_range.second - head_range.first), "\r\n");
        for (const std::string& tag : head_tag) {
            if (tag == head_tag.front()) {
                std::list<std::string> keys = strutil::split(tag, " ");
                uint_64 state{0};
                if (keys.size() >= 2 && parse_uint(*(++keys.begin()), 999, state)) {
                    m_reply->m_version = alx::map_value(version_string2key, keys.front(), version::NONE);
                    m_reply->m_state = (uint_16) state;
                } else {
                    mesg_prit.exec("HTTP head info error!");
                    success = false;
                }
            } else {
                std::list<std::string> keys = strutil::split(tag, ": ");
                if (keys.size() == 2) m_reply->m_field[keys.front()] = keys.back();
                else {
                    mesg_prit.exec("HTTP head field error!");
                    success = false;
                }
            }
        }
        if (!success) {
            safe_delete(m_reply);
            return;
        }
        m_reply->m_alive = alx::map_value(m_reply->m_field, std::string("Connection")) == "keep-alive";
        m_buffer = m_buffer.mid(head_range.second + 4);
    }

    const std::string content_size = map_value<std::string>(m_reply->m_field, "Content-Length");
    const std::string transfer_encoding = map_value<std::string>(m_reply->m_field, "Transfer-Encoding");
    if (!content_size.empty()) {
        uint_64 size{0};
        if (!parse_uint(content_size, max_uint_64, size)) {
            mesg_prit.exec("HTTP head parse \"Content-Length\" failed!");
            return;
        }
        if (m_buffer.size() >= size) m_reply->m_content = m_buffer.left(size);
        else return;
    } else if (!transfer_encoding.empty()) {
        if (transfer_encoding == "chunked") {
            bool need_more{false};
            bytes content;
            if (!decode_chunked(m_buffer, content, need_more)) {
                if (need_more) return;
                mesg_prit.exec("HTTP head parse \"Transfer-Encoding\" failed!");
                m_reply->m_content.clear();
            } else m_reply->m_content = std::move(content);
        } else mesg_prit.exec("HTTP head parse \"Transfer-Encoding\" failed!");
    }

    auto it = m_reply->m_field.find("Content-Encoding");
    if (it != m_reply->m_field.end() && it->second.find("gzip") != std::string::npos && !m_reply->m_content.empty()) {
        bytes decompressed = compress::decoder_gzip::sexec(m_reply->m_content);
        if (!decompressed.empty()) {
            m_reply->m_content = std::move(decompressed);
            m_reply->m_field.erase(it);
        }
    }

    m_buffer.clear();
    m_promise->set_value(std::move(*m_reply));
    safe_delete(m_reply);
    safe_delete(m_promise);
}

void client::on_trans_event(uint_64, bool _connect) {
    if (_connect) return;
    // queued sends belong to the dead connection: their answers would land on the next request
    clear();
    if (abort_pending()) mesg_prit.exec("pending request aborted: connection closed");
}

bool client::abort_pending() {
    std::promise<reply>* pending = m_promise;
    m_promise = nullptr;
    safe_delete(m_reply);
    m_buffer.clear();
    if (nullptr == pending) return false;
    // fulfil only after detaching the state: a caller that reissues the moment it wakes is not refused
    pending->set_value(reply());
    delete pending;
    return true;
}
