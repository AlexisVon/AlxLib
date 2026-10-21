#include "filesys.h"
#include "afile.h"

#ifdef _WIN32
#    include <windows.h>
// windows.h defines these as macros; they would rewrite alx::http::action::DELETE et al.
#    undef DELETE
#    undef OPTIONS
#    undef TRACE
#else
#    include <dirent.h>
#endif
#include <vector>

namespace {
    constexpr alx::uint_64 block_size{4 * 1024 * 1024};

    std::string g_root;
    std::string g_root_name{"Root"};

    alx::http::reply fail(const alx::http::request& _req, alx::uint_16 _state, const char* _code) {
        alx::json_object json;
        json.insert("error", std::string(_code));
        alx::http::reply reply(_req.get_version(), _state);
        reply.set_content(json);
        return reply;
    }

    alx::http::reply ok_json(const alx::http::request& _req, alx::json_object& _json) {
        alx::http::reply reply(_req.get_version(), alx::http::state::OK);
        reply.set_content(_json);
        return reply;
    }

    std::string to_ansi(const std::string& _utf8) {
#ifdef _WIN32
        return alx::strutil::code_conver(_utf8, alx::strutil::GBK, alx::strutil::UTF8);
#else
        return _utf8;
#endif
    }

    std::string to_web(const std::string& _native) {
        return alx::strutil::code_conver(_native, alx::strutil::UTF8, alx::strutil::AUTO);
    }

    std::string to_native(const std::string& _web) {
        std::string path = to_ansi(_web);
        if (g_root.empty()) {
#ifdef _WIN32
            if (path.size() >= 2 && '/' == path[0]) {
                path[0] = path[1];
                path[1] = ':';
            }
            if (2 == path.size() && ':' == path[1]) path += '/';
#endif
            return path;
        }
        if ("/" == g_root) return path;
        if ("/" == path) return g_root + '/';
        return g_root + path;
    }

    std::string base_name(const std::string& _path) {
        std::string path = _path;
        while (path.size() > 1 && ('/' == path.back() || '\\' == path.back())) {
            if (3 == path.size() && ':' == path[1]) break;
            path.pop_back();
        }
        if (2 == path.size() && ':' == path[1]) return path;
        if (3 == path.size() && ':' == path[1]) return path.substr(0, 2);
        const std::size_t pos = path.find_last_of("/\\");
        if (std::string::npos == pos) return path.empty() ? std::string("/") : path;
        const std::string name = path.substr(pos + 1);
        return name.empty() ? path : name;
    }

    bool safe_web(const std::string& _path) {
        if (_path.empty() || '/' != _path[0]) return false;
#ifdef _WIN32
        if (std::string::npos != _path.find('\\')) return false;
#endif
        std::size_t begin{0};
        while (begin <= _path.size()) {
            std::size_t end = _path.find('/', begin);
            if (std::string::npos == end) end = _path.size();
            const std::string part = _path.substr(begin, end - begin);
            if ("." == part || ".." == part) return false;
            if (end == _path.size()) break;
            begin = end + 1;
        }
        return true;
    }

    bool valid_name(const std::string& _name) {
        if (_name.empty() || "." == _name || ".." == _name) return false;
        for (const char ch : _name)
            if ('/' == ch || '\\' == ch || '\0' == ch) return false;
        return true;
    }

    bool parse_block(const std::string& _text, alx::uint_64& _value) {
        constexpr alx::uint_64 limit{0XFFFFFFFFULL};
        alx::uint_64 value{0};
        if (_text.empty()) return true;
        for (const char ch : _text) {
            if (ch < '0' || ch > '9') return false;
            if (value > (limit - 9) / 10) return false;
            value = value * 10 + (ch - '0');
        }
        _value = value;
        return true;
    }

    std::vector<std::string> list_drivers() {
        std::vector<std::string> drives;
#ifdef _WIN32
        const DWORD mask = GetLogicalDrives();
        for (int i = 0; i < 26; ++i)
            if (mask & (1U << i)) drives.emplace_back(1, static_cast<char>('A' + i));
#else
        static constexpr const char* common[] = {"/home", "/usr", "/mnt", "/var", "/opt", "/tmp", "/etc", "/boot"};
        for (const char* path : common) {
            const alx::file_info info(path);
            if (info.is_exist() && info.is_dir()) drives.push_back(info.name());
        }
#endif
        return drives;
    }

    void append_child(alx::json_object& _json, const alx::file_info& _child) {
        if (_child.is_link()) return;
        alx::json_object item;
        item.insert("type", std::string(_child.is_dir() ? "folder" : "file"));
        if (!_child.is_dir()) item.insert("size", static_cast<alx::int_64>(_child.size()));
        item.insert("last_write_time", alx::file_info::conver_file_time(_child.time_lw()).to_string());
        _json.insert(to_web(_child.name()), item);
    }

    void append_children(alx::json_object& _json, const alx::file_info& _dir) {
#ifndef _WIN32
        // file_info drops the trailing separator, so the POSIX root ends up with an empty path
        if (_dir.path().empty() && _dir.is_dir()) {
            DIR* dir = opendir("/");
            if (nullptr == dir) return;
            struct dirent* entry = nullptr;
            while ((entry = readdir(dir)) != nullptr) {
                const std::string name(entry->d_name);
                if ("." != name && ".." != name) append_child(_json, alx::file_info("/" + name));
            }
            closedir(dir);
            return;
        }
#endif
        for (const alx::file_info& child : _dir.get_child()) append_child(_json, child);
    }
}

bool filesys::set_root(const std::string& _path, std::string& _error) {
    g_root.clear();
    g_root_name = "Root";
    if (_path.empty()) return true;

    const alx::file_info info(_path);
    if (!info.is_exist()) {
        _error = "root directory not found: " + _path;
        return false;
    }
    if (!info.is_dir()) {
        _error = "root is not a directory: " + _path;
        return false;
    }

    g_root = info.path();
#ifdef _WIN32
    if (2 == g_root.size() && ':' == g_root[1]) g_root += '/';
#else
    if (g_root.empty()) g_root = "/";
#endif
    while (g_root.size() > 1 && ('/' == g_root.back() || '\\' == g_root.back())) {
        if (3 == g_root.size() && ':' == g_root[1]) break;
        g_root.pop_back();
    }
    g_root_name = base_name(g_root);
    if ("/" == g_root_name || g_root_name.empty()) g_root_name = "Root";
    return true;
}

alx::http::api* filesys::new_api() {
    alx::http::api* api = new alx::http::api(nullptr);
    api->insert(new alx::http::api(root), "root");
    api->insert(new alx::http::api(list), "list");
    api->insert(new alx::http::api(read), "read");
    api->insert(new alx::http::api(upload), "upload");
    api->insert(new alx::http::api(remove), "remove");
    return api;
}

alx::http::reply filesys::root(const alx::http::request& _req) {
    if (alx::http::action::GET != _req.get_action())
        return fail(_req, alx::http::state::FORBIDDEN, "forbidden");

    alx::json_object json;
    json.insert("virtual", g_root.empty());
    json.insert("name", g_root.empty() ? std::string("Root") : to_web(g_root_name));
    json.insert("path", g_root.empty() ? std::string("/") : to_web(g_root));
    return ok_json(_req, json);
}

alx::http::reply filesys::list(const alx::http::request& _req) {
    if (alx::http::action::GET != _req.get_action())
        return fail(_req, alx::http::state::FORBIDDEN, "forbidden");

    const std::string path = alx::map_value<std::string, std::string>(_req.get_param(), "path", "/");
    if (!safe_web(path)) return fail(_req, alx::http::state::BAD_REQUEST, "bad_path");

    alx::json_object json;
    if ("/" == path && g_root.empty()) {
        for (const std::string& drive : list_drivers()) {
            alx::json_object item;
            item.insert("type", std::string("disk"));
            json.insert(drive, item);
        }
    } else {
        const alx::file_info dir(to_native(path));
        if (!dir.is_exist()) return fail(_req, alx::http::state::NOT_FOUND, "not_found");
        if (!dir.is_dir()) return fail(_req, alx::http::state::BAD_REQUEST, "not_dir");
        append_children(json, dir);
    }
    return ok_json(_req, json);
}

alx::http::reply filesys::read(const alx::http::request& _req) {
    if (alx::http::action::GET != _req.get_action())
        return fail(_req, alx::http::state::FORBIDDEN, "forbidden");

    const std::string path = alx::map_value<std::string, std::string>(_req.get_param(), "path", "");
    if (!safe_web(path)) return fail(_req, alx::http::state::BAD_REQUEST, "bad_path");

    alx::uint_64 block{0};
    if (!parse_block(alx::map_value<std::string, std::string>(_req.get_field(), "FileBlockIndex", "0"), block))
        return fail(_req, alx::http::state::BAD_REQUEST, "bad_request");

    const alx::file_info info(to_native(path));
    if (!info.is_exist()) return fail(_req, alx::http::state::NOT_FOUND, "not_found");
    if (info.is_link()) return fail(_req, alx::http::state::NOT_FOUND, "is_link");
    if (info.is_dir()) return fail(_req, alx::http::state::BAD_REQUEST, "not_file");

    const alx::uint_64 total = 0 == info.size() ? 1 : (info.size() + block_size - 1) / block_size;
    if (block >= total) return fail(_req, alx::http::state::BAD_REQUEST, "block_out_of_range");

    alx::file file;
    if (!file.open(info, alx::file::READ))
        return fail(_req, alx::http::state::INTERNAL_SERVER_ERROR, "open_failed");

    const alx::bytes data = file.read(block * block_size, block_size);
    alx::http::reply reply(_req.get_version(), alx::http::state::OK);
    reply.set_content(alx::http::mime::oct, alx::bytes_view(data));
    reply.set_field("FileBlockIndex", std::to_string(block));
    reply.set_field("FileBlockTotal", std::to_string(total));
    return reply;
}

alx::http::reply filesys::upload(const alx::http::request& _req) {
    if (alx::http::action::POST != _req.get_action())
        return fail(_req, alx::http::state::FORBIDDEN, "forbidden");

    const std::string dir_web = alx::map_value<std::string, std::string>(_req.get_param(), "path", "");
    const std::string name = alx::map_value<std::string, std::string>(_req.get_param(), "name", "");
    const bool overwrite = "1" == alx::map_value<std::string, std::string>(_req.get_param(), "overwrite", "0");
    if (!safe_web(dir_web) || !valid_name(name))
        return fail(_req, alx::http::state::BAD_REQUEST, "bad_name");
    if ("/" == dir_web && g_root.empty())
        return fail(_req, alx::http::state::BAD_REQUEST, "bad_path");

    std::string base = to_native(dir_web);
    while (base.size() > 1 && ('/' == base.back() || '\\' == base.back())) {
        if (3 == base.size() && ':' == base[1]) break;
        base.pop_back();
    }
    const alx::file_info dir(base);
    if (!dir.is_exist()) return fail(_req, alx::http::state::NOT_FOUND, "not_found");
    if (!dir.is_dir()) return fail(_req, alx::http::state::BAD_REQUEST, "not_dir");

    std::string target_path = base;
    if (target_path.empty() || ('/' != target_path.back() && '\\' != target_path.back()))
        target_path += '/';
    target_path += to_ansi(name);

    const alx::file_info target(target_path);
    if (target.is_link()) return fail(_req, alx::http::state::CONFLICT, "is_link");
    if (target.is_exist()) {
        if (target.is_dir()) return fail(_req, alx::http::state::CONFLICT, "is_dir");
        if (!overwrite) return fail(_req, alx::http::state::CONFLICT, "exists");
    }
    if (!alx::file::write_all(target_path, _req.get_content(), false))
        return fail(_req, alx::http::state::INTERNAL_SERVER_ERROR, "write_failed");

    alx::json_object json;
    json.insert("ok", true);
    json.insert("size", static_cast<alx::int_64>(_req.get_content().size()));
    return ok_json(_req, json);
}

alx::http::reply filesys::remove(const alx::http::request& _req) {
    if (alx::http::action::DELETE != _req.get_action())
        return fail(_req, alx::http::state::FORBIDDEN, "forbidden");

    const std::string web = alx::map_value<std::string, std::string>(_req.get_param(), "path", "");
    if (!safe_web(web) || "/" == web)
        return fail(_req, alx::http::state::BAD_REQUEST, "bad_path");

    const alx::file_info info(to_native(web));
    if (!info.is_exist()) return fail(_req, alx::http::state::NOT_FOUND, "not_found");
    if (info.is_link()) return fail(_req, alx::http::state::CONFLICT, "is_link");

    if (info.is_dir()) {
        if (!info.get_child().empty()) return fail(_req, alx::http::state::CONFLICT, "not_empty");
        if (!alx::file_info::rmdir(info.path()))
            return fail(_req, alx::http::state::INTERNAL_SERVER_ERROR, "remove_failed");
    } else if (!alx::file_info::rmfile(info.path())) {
        return fail(_req, alx::http::state::INTERNAL_SERVER_ERROR, "remove_failed");
    }

    alx::json_object json;
    json.insert("ok", true);
    return ok_json(_req, json);
}
