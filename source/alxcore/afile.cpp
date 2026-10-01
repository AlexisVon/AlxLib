/*****************************************************************/ /**
 * \file   afile.cpp
 * \brief  File system operations (file info retrieval, directory management, file operations)
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "afile.h"
#include "aplatform.h"

using namespace alx;

// 64-bit positions: MSVC's ftell/fseek are 32-bit, so both branches take the 64-bit twins
#ifdef _WIN32
#    pragma warning(disable : 4996)
#    include <Shlwapi.h>
#    pragma comment(lib, "Shlwapi.lib")
#    define FTELL64 _ftelli64
#    define FSEEK64 _fseeki64
#else
#    define FTELL64 ftello64
#    define FSEEK64 fseeko64
#endif
alx::file_info::file_info(const std::string& _path)
    : path_(conver_std_path(_path)) {
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA info;
    if (exist_ = GetFileAttributesExA(path_.c_str(), GetFileExInfoStandard, &info)) {
        dir_ = info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
        link_ = info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT;
        size_ = (uint_64(info.nFileSizeHigh) << 32) | uint_64(info.nFileSizeLow);

        time_ct_ = (uint_64(info.ftCreationTime.dwHighDateTime) << 32) | uint_64(info.ftCreationTime.dwLowDateTime);
        time_la_ = (uint_64(info.ftLastAccessTime.dwHighDateTime) << 32) | uint_64(info.ftLastAccessTime.dwLowDateTime);
        time_lw_ = (uint_64(info.ftLastWriteTime.dwHighDateTime) << 32) | uint_64(info.ftLastWriteTime.dwLowDateTime);
    }
    if (dir_ && '\\' == path_.back()) path_.pop_back();

    PSTR name_c = PathFindFileNameA(path_.c_str());
    if (name_c != nullptr) name_ = name_c;
    PSTR suffix_c = PathFindExtensionA(path_.c_str());
    if (suffix_c != nullptr) suffix_ = suffix_c;
#else
    // realpath() in conver_std_path() resolves the link chain away, so the link test runs on the raw path;
    // stat() would resolve it too, which is why S_ISLNK on its result is never true
    struct stat linfo;
    if (0 == lstat(_path.c_str(), &linfo)) link_ = S_ISLNK(linfo.st_mode);

    struct stat info;
    if ((exist_ = (stat(path_.c_str(), &info) == 0))) {
        dir_ = S_ISDIR(info.st_mode);
        size_ = static_cast<uint_64>(info.st_size);

        time_ct_ = static_cast<uint_64>(info.st_ctime);
        time_la_ = static_cast<uint_64>(info.st_atime);
        time_lw_ = static_cast<uint_64>(info.st_mtime);
    }
    // only the root gets here (realpath drops other trailing separators), leaving path_ empty and is_valid() false
    if (dir_ && '/' == path_.back()) path_.pop_back();

    size_t pos = path_.find_last_of('/');
    if (pos != std::string::npos) name_ = path_.substr(pos + 1);
    pos = name_.find_last_of('.');
    if (pos != std::string::npos) suffix_ = name_.substr(pos);
#endif
}

file_info alx::file_info::get_parent() const {
    if (name_.empty()) return file_info();
#ifdef _WIN32
    return file_info(strutil::left(path_, strutil::rfind(path_, "\\")));
#else
    size_t pos = path_.find_last_of('/');
    if (pos == std::string::npos) return file_info();
    else return file_info(path_.substr(0, pos));
#endif
}

std::vector<file_info> alx::file_info::get_child(const std::string& _filter) const {
    std::vector<file_info> result;
    if (!exist_ || !dir_) return result;

#ifdef _WIN32
    std::string seach = path_ + '\\' + _filter;
    WIN32_FIND_DATAA find_file;
    HANDLE h_find = FindFirstFileA(seach.c_str(), &find_file);
    if (h_find == INVALID_HANDLE_VALUE) return result;

    do {
        if (strcmp(find_file.cFileName, ".") != 0 && strcmp(find_file.cFileName, "..") != 0)
            result.emplace_back(file_info(path_ + '\\' + std::string(find_file.cFileName)));
    } while (FindNextFileA(h_find, &find_file) != 0);

    FindClose(h_find);
#else
    DIR* dir = opendir(path_.c_str());
    if (!dir) return result;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name(entry->d_name);
        if (name == "." || name == "..") continue;
        if (_filter.empty() || fnmatch(_filter.c_str(), name.c_str(), 0) == 0)
            result.emplace_back(file_info(path_ + '/' + name));
    }
    closedir(dir);
#endif
    return result;
}

bool alx::file_info::mkdir() {
    return exist_ ? dir_ : mkdir(path_) ? (exist_ = dir_ = true)
                                        : false;
}

bool alx::file_info::mkdir(const std::string& _path) {
#ifdef _WIN32
    return CreateDirectoryA(_path.c_str(), nullptr) != 0;
#else
    return ::mkdir(_path.c_str(), 0755) == 0;
#endif
}

bool alx::file_info::rmdir(const std::string& _path) {
#ifdef _WIN32
    return RemoveDirectoryA(_path.c_str()) != 0;
#else
    return ::rmdir(_path.c_str()) == 0;
#endif
}

bool alx::file_info::rmfile(const std::string& _path) {
#ifdef _WIN32
    return DeleteFileA(_path.c_str()) != 0;
#else
    return ::unlink(_path.c_str()) == 0;
#endif
}

bool alx::file_info::mvfile(const std::string& _from, const std::string& _to) {
#ifdef _WIN32
    return MoveFileA(_from.c_str(), _to.c_str());
#else
    return rename(_from.c_str(), _to.c_str()) == 0;
#endif
}

uint_64 alx::file_info::calsize(const file_info& _info, bool _ignore_link) {
    // POSIX never sets is_link() (stat() follows links), so a link to an ancestor is not stopped here
    if (!_info.is_valid() || (_ignore_link && _info.is_link())) return 0;
    uint_64 result{0};
    if (_info.is_dir()) {
        for (const file_info& it : _info.get_child())
            if (it.is_dir()) result += calsize(it, _ignore_link);
            else result += it.size_;
    } else result += _info.size_;

    return result;
}

datetime alx::file_info::conver_file_time(uint_64 _time) {
#ifdef _WIN32
    // 116444736000000000 is the 1601-to-1970 offset in 100 ns ticks, the FILETIME epoch
    return datetime(std::chrono::time_point<std::chrono::system_clock>(
        std::chrono::duration<uint_64, std::ratio<1, 10000000>>(_time - 116444736000000000LLU)));
#else
    return datetime(std::chrono::time_point<std::chrono::system_clock>(
        std::chrono::seconds(_time)));
#endif
}

std::string alx::file_info::conver_std_path(const std::string& _path) {
    if (_path.empty()) return std::string();
#ifdef _WIN32
    DWORD required_length = GetFullPathNameA(_path.c_str(), 0, nullptr, nullptr);
    if (required_length > 0) {
        std::vector<char> buf(required_length);
        return GetFullPathNameA(_path.c_str(), required_length, buf.data(), nullptr) != 0 ? std::string(buf.data()) : _path;
    }
    return _path;
#else
    char* cpath = realpath(_path.c_str(), nullptr);
    if (nullptr != cpath) {
        std::string path = cpath;
        free(cpath);
        return path;
    }
    if (_path[0] == '/') return _path;
    char* cwd = getcwd(nullptr, 0);
    if (nullptr == cwd) return _path;
    std::string abs_path = std::string(cwd) + "/" + _path;
    free(cwd);
    cpath = realpath(abs_path.c_str(), nullptr);
    if (nullptr != cpath) {
        std::string path = cpath;
        free(cpath);
        return path;
    }
    return abs_path;
#endif
}

namespace file_impl {
    inline FILE* open(const char* _path, const char* _mode) { return fopen(_path, _mode); }

    inline void set_ok(bool* _ok, bool _value) {
        if (nullptr != _ok) *_ok = _value;
    }

    inline bool is_seekable(FILE* _file) { return FSEEK64(_file, 0, SEEK_CUR) == 0; }

    // owns the stream: every path but the nullptr one ends in fclose
    inline bytes read_all(FILE* _file, bool& _ok) {
        _ok = false;
        bytes data;
        if (nullptr == _file) return data;

        // the seek-to-end size is a growth hint only: procfs files report 0 while holding content
        uint_64 hint{0};
        if (is_seekable(_file) && fseek(_file, 0, SEEK_END) == 0) {
            const int_64 end = (int_64) FTELL64(_file);
            if (end > 0) hint = (uint_64) end;
            fseek(_file, 0, SEEK_SET);
        }
        if (hint > 0) data.resize(hint);

        uint_64 used{0};
        for (;;) {
            if (used == data.size()) {
                data.resize(data.size() ? data.size() << 1 : 0X10000U);
                if (data.size() <= used) break;
            }
            const uint_64 cnt = (uint_64) fread(data.data() + used, 1, data.size() - used, _file);
            used += cnt;
            if (0 == cnt) break;
        }
        _ok = 0 == ferror(_file);
        data.resize(used);
        data.fitsize();
        fclose(_file);
        return data;
    }

    inline bool write_stream(FILE* _file, const void* _data, const uint_64 _size) {
        const uint_8* ptr = (const uint_8*) _data;
        uint_64 written = 0;
        while (written < _size) {
            const uint_64 cnt = (uint_64) fwrite(ptr + written, 1, _size - written, _file);
            if (0 == cnt) return false;
            written += cnt;
        }
        return true;
    }

    inline bool write_all(FILE* _file, const void* _data, const uint_64 _size) {
        if (nullptr == _file) return false;
        const bool done = write_stream(_file, _data, _size);
        return (fclose(_file) != EOF) && done;
    }
}

bytes alx::file::read_all(const file_info& _info, bool* _ok) {
    if (_info.is_dir() || !_info.is_exist()) return file_impl::set_ok(_ok, false), bytes();
    bool ok{false};
    bytes data = file_impl::read_all(file_impl::open(_info.path().c_str(), "rb"), ok);
    return file_impl::set_ok(_ok, ok), data;
}

bytes alx::file::read_all(const std::string& _path, bool* _ok) {
    bool ok{false};
    bytes data = file_impl::read_all(file_impl::open(_path.c_str(), "rb"), ok);
    return file_impl::set_ok(_ok, ok), data;
}

bool alx::file::write_all(const file_info& _info, const void* _data, uint_64 _size, bool _append) {
    return _info.is_dir() ? false : file_impl::write_all(file_impl::open(_info.path().c_str(), _append ? "ab" : "wb"), _data, _size);
}

bool alx::file::write_all(const std::string& _path, const void* _data, uint_64 _size, bool _append) {
    return file_impl::write_all(file_impl::open(_path.c_str(), _append ? "ab" : "wb"), _data, _size);
}

bool alx::file::open(const file_info& _info, uint_8 _type) {
    static const std::unordered_map<uint_8, const char*> mode_map{
        {READ, "rb"}, {WRIT, "wb"}, {APED, "ab"}, {READ | R__W, "rb+"}, {WRIT | R__W, "wb+"}, {APED | R__W, "ab+"}};
    // _info may alias m_info (open(f.info(), ...)): copy it before close() clears the object
    const file_info info = _info;
    if (is_open()) close();
    if (info.is_dir()) return false;

    const char* mode = alx::map_value(mode_map, _type);
    if (nullptr == mode || (mode[0] == L'r' && !info.is_exist())) return false;

    m_file = file_impl::open(info.path().c_str(), mode);
    if (m_file == nullptr) return false;
    m_info = info;
    m_type = _type;
    // "wb"/"wb+" truncated the file on open, so the recorded size follows at once
    if (WRIT & _type) m_info.set_size(0);
    return true;
}

bytes alx::file::read(uint_64 _pos, uint_64 _size, bool* _ok) const {
    if (!is_read_able() || !file_impl::is_seekable(m_file)) return file_impl::set_ok(_ok, false), bytes();

    // bounded by m_info, not by the stream: bytes appended after open() are out of reach
    if (_pos >= m_info.size()) return file_impl::set_ok(_ok, true), bytes();
    if (_size == uint_64_npos || _size + _pos > m_info.size()) _size = m_info.size() - _pos;
    if (_pos != (uint_64) FTELL64(m_file) && FSEEK64(m_file, _pos, SEEK_SET) != 0) return file_impl::set_ok(_ok, false), bytes();
    bytes data(_size);
    data.resize(fread(data.data(), 1, data.size(), m_file));
    return file_impl::set_ok(_ok, 0 == ferror(m_file)), data;
}

bool alx::file::write(const void* _data, uint_64 _size, uint_64 _pos) {
    if (!is_write_able()) return false;

    if (_pos != uint_64_npos && FSEEK64(m_file, _pos, SEEK_SET) != 0) return false;
    if (!file_impl::write_stream(m_file, _data, _size)) return false;

    const int_64 end = (int_64) FTELL64(m_file);
    if (end > 0 && (uint_64) end > m_info.size()) m_info.set_size(end);
    return true;
}

bool alx::file::flush() {
    if (!is_write_able()) return true;

    return 0 == fflush(m_file);
}

void alx::file::close() {
    if (m_file) {
        fclose(m_file);
        m_file = nullptr;
    }
    m_info = file_info();
    m_type = IO_TYPE::NONE;
}
