/*****************************************************************/ /**
 * \file   aplatform.cpp
 * \brief  Cross-platform adaptation layer (Windows/Linux)
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "aplatform.h"
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <fcntl.h>

using namespace std;
using namespace alx;

#define __MMAP_MEMORY_LOCK__          \
    mmap_mutex_locker locker(pmutex); \
    (void) locker

#ifdef _WIN32
alx::mmap::mmap(const std::string& _name)
    : name(_name), size(0), map(nullptr), pmutex(nullptr), data(nullptr) {
}
#else
alx::mmap::mmap(const std::string& _name)
    : name(_name), size(0), fd(-1), pmutex(nullptr), data(nullptr) {
}
#endif

alx::mmap::~mmap() {

    if (opened() && owner()) destroy();
    else close();
}

bool alx::mmap::open(uint_32 _size, bool _create) {
    if (opened()) return true;
    if (name.empty() || 0 == _size) return false;
    __MMAP_MEMORY_LOCK__;
#ifdef _WIN32
    string mname = name + "_mutex";

    if (nullptr == (map = OpenFileMappingA(FILE_MAP_ALL_ACCESS, 0, name.c_str()))) {
        map = _create ? CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, _size, name.c_str()) : nullptr;
        if (nullptr == map) goto FAILURE;

        // the mapping may have appeared between the two calls, so this is true only when we made it
        created = (ERROR_ALREADY_EXISTS != GetLastError());
    }
    if (nullptr == (pmutex = OpenMutexA(MUTEX_ALL_ACCESS, FALSE, mname.c_str()))) {
        pmutex = _create ? CreateMutexA(NULL, FALSE, mname.c_str()) : nullptr;
        if (nullptr == pmutex) goto FAILURE;
    }
    if (nullptr == (data = MapViewOfFile(map, FILE_MAP_ALL_ACCESS, 0, 0, 0))) goto FAILURE;

    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(data, &mbi, sizeof(mbi)) == 0) goto FAILURE;
    if (mbi.RegionSize < _size) goto FAILURE;

    size = (uint_32) mbi.RegionSize;
    return true;
FAILURE:
    if (nullptr != data) {
        UnmapViewOfFile(data);
        data = nullptr;
    }
    if (nullptr != map) {
        CloseHandle(map);
        map = nullptr;
    }
    if (nullptr != pmutex) {
        CloseHandle(pmutex);
        pmutex = nullptr;
    }
    size = 0;
    created = false;
    return false;
#else
    std::string shm_name = "/" + name;
    std::string sem_name = "/" + name + "_sem";
    int tfd = -1;
    bool fresh = false;
    void* mapped_data{nullptr};

    // O_EXCL makes creation the atomic decision: the winner truncates, a late creator only attaches
    tfd = ::shm_open(shm_name.c_str(), _create ? (O_CREAT | O_EXCL | O_RDWR) : O_RDWR, S_IRUSR | S_IWUSR | S_IWGRP | S_IRGRP);
    if (-1 != tfd) {
        fresh = _create;
    } else if (EEXIST == errno && _create) {
        tfd = ::shm_open(shm_name.c_str(), O_RDWR, S_IRUSR | S_IWUSR | S_IWGRP | S_IRGRP);
    }
    if (-1 == tfd) goto FAILURE;

    if (fresh) {
        if (-1 == ::ftruncate(tfd, _size)) {
            ::shm_unlink(shm_name.c_str());
            ::close(tfd);
            goto FAILURE;
        }
    } else {
        struct stat st;

        // an attacher must fit inside the block that is already there, else the tail faults
        if (-1 == ::fstat(tfd, &st) || st.st_size < static_cast<off_t>(_size)) {
            ::close(tfd);
            goto FAILURE;
        }
    }

    mapped_data = ::mmap(nullptr, _size, PROT_READ | PROT_WRITE, MAP_SHARED, tfd, 0);
    if (mapped_data == MAP_FAILED) {
        if (fresh) ::shm_unlink(shm_name.c_str());
        ::close(tfd);
        goto FAILURE;
    }
    pmutex = ::sem_open(sem_name.c_str(), O_CREAT, S_IRUSR | S_IWUSR | S_IWGRP | S_IRGRP, 1);
    if (pmutex == SEM_FAILED) {
        ::munmap(mapped_data, _size);
        if (fresh) ::shm_unlink(shm_name.c_str());
        ::close(tfd);
        goto FAILURE;
    }
    this->fd = tfd;
    this->data = mapped_data;
    this->size = _size;
    this->created = fresh;
    return true;
FAILURE:
    this->fd = -1;
    this->data = nullptr;
    this->size = 0;
    this->created = false;
    return false;
#endif
}

bool alx::mmap::opened() {
    return nullptr != data;
}

bool alx::mmap::owner() const {
    return created;
}

void alx::mmap::close() {
    __MMAP_MEMORY_LOCK__;
#ifdef _WIN32
    if (nullptr != data) {
        UnmapViewOfFile(data);
        data = nullptr;
    }
    if (nullptr != map) {
        CloseHandle(map);
        map = nullptr;
    }
    if (nullptr != pmutex) {
        CloseHandle(pmutex);
        pmutex = nullptr;
    }
#else
    if (nullptr != data) {
        ::munmap(data, size);
        data = nullptr;
    }
    if (-1 != fd) {
        ::close(fd);
        fd = -1;
    }
    if (nullptr != pmutex) {
        ::sem_close(pmutex);
        pmutex = nullptr;
    }
#endif
    size = 0;
    created = false;
}

bool alx::mmap::destroy() {
    if (name.empty()) return false;
    close();
#ifdef _WIN32

    return true;
#else

    bool ok = (0 == ::shm_unlink(("/" + name).c_str()) || ENOENT == errno);
    ok = (0 == ::sem_unlink(("/" + name + "_sem").c_str()) || ENOENT == errno) && ok;
    return ok;
#endif
}

// the shared locked bounds check; 0 bytes is a no-op, and the subtraction form cannot wrap
#define MMAP_PROTECT(_OFFSET_, _SIZE_) \
    if (0 == _SIZE_) return true;      \
    __MMAP_MEMORY_LOCK__;              \
    if (_OFFSET_ > size || _SIZE_ > size - _OFFSET_) return false;

bool alx::mmap::set(uint_8 _value, uint_32 _offset) {
    MMAP_PROTECT(_offset, 1);
    static_cast<uint_8*>(data)[_offset] = _value;
    return true;
}

bool alx::mmap::set(uint_8 _value, uint_32 _offset, uint_32 _size) {
    MMAP_PROTECT(_offset, _size);
    memset(static_cast<uint_8*>(data) + _offset, _value, _size);
    return true;
}

bool alx::mmap::cas(uint_8 _expected, uint_8 _replace, uint_32 _offset) {
    MMAP_PROTECT(_offset, 1);
    const uint_8 old = static_cast<uint_8*>(data)[_offset];
    if (old != _expected) return false;
    static_cast<uint_8*>(data)[_offset] = _replace;
    return true;
}

bool alx::mmap::write(const void* _source, uint_32 _offset, uint_32 _size) {
    MMAP_PROTECT(_offset, _size);
    memcpy(static_cast<uint_8*>(data) + _offset, _source, _size);
    return true;
}

bool alx::mmap::read(void* _buffer, uint_32 _offset, uint_32 _size) {
    MMAP_PROTECT(_offset, _size);
    memcpy(_buffer, static_cast<uint_8*>(data) + _offset, _size);
    return true;
}

bool alx::mmap::take(void* _buffer, uint_32 _offset, uint_32 _size) {
    MMAP_PROTECT(_offset, _size);
    memcpy(_buffer, static_cast<uint_8*>(data) + _offset, _size);
    memset(static_cast<uint_8*>(data) + _offset, 0, _size);
    return true;
}

void alx::mmap::clear() {
    if (!opened()) return;
    __MMAP_MEMORY_LOCK__;
    memset(static_cast<uint_8*>(data), 0, size);
}

exec_result
alx::exec_sync(
    const std::string& _cmd,
    const uint_32 _mill_wait) {
    return exec_sync(_cmd, ".", _mill_wait);
}

exec_result
alx::exec_sync(
    const std::string& _cmd, const std::string& _dir,
    const uint_32 _mill_wait) {
    constexpr size_t mini_wait = 60;
    constexpr size_t buffer_size = 4096;
    if (_cmd.empty()) return {true, false, 0};

    bool success = false;
    std::atomic_bool timeout(false);
    std::atomic_bool execing(false);
    std::string result;
    std::thread* wait_thread{nullptr};
    int_32 excode = -1;
#ifdef _WIN32
    HANDLE hReadPipe = NULL;
    HANDLE hWritePipe = NULL;
    PROCESS_INFORMATION pi;
    STARTUPINFOA si;
    SECURITY_ATTRIBUTES sa;

    string cmd = _cmd;
    string dir = _dir;

    char buffer[buffer_size];
    DWORD count = 0;
    memset(&pi, NULL, sizeof(pi));
    memset(&si, NULL, sizeof(pi));
    memset(&sa, NULL, sizeof(pi));

    pi.hProcess = NULL;
    pi.hThread = NULL;
    si.cb = sizeof(STARTUPINFOA);
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    if (FALSE == CreatePipe(&hReadPipe, &hWritePipe, &sa, NULL)) {
        success = false;
        result = "ERROR: create pipe " + std::to_string(GetLastError());
        goto RET;
    }
    GetStartupInfoA(&si);
    si.hStdError = hWritePipe;
    si.hStdOutput = hWritePipe;
    si.wShowWindow = SW_HIDE;
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;

    if (FALSE == CreateProcessA(NULL,
                                &cmd[0], NULL, NULL, true,
                                0, NULL, dir.c_str(), &si, &pi)) {
        success = false;
        result = "ERROR: create process " + std::to_string(GetLastError());
        goto RET;
    }
    CloseHandle(hWritePipe);
    hWritePipe = NULL;
    execing = true;

    wait_thread = new std::thread(
        [&]() {
            uint_32 total_wait{0};
            while (total_wait < _mill_wait) {
                std::this_thread::sleep_for(std::chrono::milliseconds(mini_wait));
                if (!execing) {
                    timeout = false;
                    return;
                }
                total_wait += mini_wait;
            }
            if (execing) {
                TerminateProcess(pi.hProcess, 1);
                timeout = true;
            }
        });

    while (execing) {
        if (!ReadFile(hReadPipe, buffer, buffer_size, &count, 0) && GetLastError() != ERROR_BROKEN_PIPE) {
            success = false;
            result = "ERROR: pipe read " + std::to_string(GetLastError());
            goto RET;
        }
        if (0 != count) result.append(buffer, count);
        if (!GetExitCodeProcess(pi.hProcess, (LPDWORD) &excode)) {
            success = false;
            result = "ERROR: get exit code " + std::to_string(GetLastError());
            goto RET;
        }
        execing = (excode == STILL_ACTIVE);
    }
    success = true;

RET:
    if (nullptr != wait_thread) {
        wait_thread->join();
        delete wait_thread;
    }
    if (pi.hProcess != NULL) {
        CloseHandle(pi.hProcess);
    }
    if (pi.hThread != NULL) {
        CloseHandle(pi.hThread);
    }
    if (hWritePipe != NULL) CloseHandle(hWritePipe);
    if (hReadPipe != NULL) CloseHandle(hReadPipe);

    return {success, timeout, excode, result};
#else
    pid_t pid;
    int pipefd[2]{-1, -1};
    int status;
    if (pipe(pipefd) == -1) return {false, false, -1, "ERROR: create pipe " + std::to_string(errno)};

    pid = fork();
    if (pid == 0) {
        // its own process group, so the timeout can take the whole group down with it
        setpgid(0, 0);
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        if (chdir(_dir.c_str()) == -1) exit(1);
        execl("/bin/sh", "sh", "-c", _cmd.c_str(), nullptr);
        exit(127);
    } else if (pid > 0) {
        // both sides call it: whoever is first wins, and the group has to exist before the timeout
        // thread can signal it
        setpgid(pid, pid);
        close(pipefd[1]);
        execing = true;
        wait_thread = new std::thread(
            [&]() {
                uint_32 total_wait{0};
                while (total_wait < _mill_wait) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(mini_wait));
                    if (!execing) {
                        timeout = false;
                        return;
                    }
                    total_wait += mini_wait;
                }
                if (execing) {
                    // the group, not just the child: whatever inherited the pipe write end goes too
                    kill(-pid, SIGTERM);
                    timeout = true;
                }
            });

        char buffer[buffer_size];
        ptrdiff_t bytes_read;
        while (execing) {
            if ((bytes_read = read(pipefd[0], buffer, buffer_size)) < 0) {
                execing = false;
                success = false;
                result = "ERROR: pipe read " + std::to_string(errno);
                goto RET;
            } else if (bytes_read > 0) result.append(buffer, bytes_read);
            const pid_t wret = waitpid(pid, &status, WNOHANG);
            if (wret == 0) {
                execing = true;
            } else if (wret == pid) {
                execing = false;
                success = true;
                excode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
            } else {
                execing = false;
                success = false;
                result = "ERROR: waitpid " + std::to_string(errno);
                goto RET;
            }
        }
    } else {
        close(pipefd[1]);
        success = false;
        result = "ERROR: fork " + std::to_string(errno);
        goto RET;
    }
RET:
    if (pipefd[0] >= 0) close(pipefd[0]);
    if (nullptr != wait_thread) {
        wait_thread->join();
        delete wait_thread;
    }
    return {success, timeout, excode, result};
#endif
}

std::future<exec_result>
alx::exec_async(
    const std::string& _cmd,
    const uint_32 _mill_wait) {
    // the pointer picks one overload of exec_sync: passing the name itself would not deduce
    static exec_result (*PTR)(const std::string&, const uint_32) = exec_sync;
    return std::async(std::launch::async, PTR, _cmd, _mill_wait);
}

std::future<exec_result>
alx::exec_async(const std::string& _cmd, const std::string& _dir, const uint_32 _mill_wait) {
    static exec_result (*PTR)(const std::string&, const std::string&, const uint_32) = exec_sync;
    return std::async(std::launch::async, PTR, _cmd, _dir, _mill_wait);
}

void alx::high_precision_sleep(uint_64 _ms) {
#ifdef _WIN32
    HANDLE timer = CreateWaitableTimerA(nullptr, TRUE, nullptr);
    if (timer != nullptr) {
        LARGE_INTEGER dueTime;
        // negative = relative to now, and the unit is 100 ns
        dueTime.QuadPart = -static_cast<LONGLONG>(_ms * 10000);
        if (SetWaitableTimer(timer, &dueTime, 0, nullptr, nullptr, 0))
            WaitForSingleObject(timer, INFINITE);
        CloseHandle(timer);
    }
#else
    struct timespec req, rem;
    req.tv_sec = _ms / 1000;
    req.tv_nsec = (_ms % 1000) * 1000000;
    while (nanosleep(&req, &rem) == -1 && errno == EINTR) req = rem;
#endif
}

#ifdef _WIN32
#    pragma comment(lib, "version.lib")
#    pragma warning(disable : 6388)
std::vector<uint_32> alx::dll_version(const std::string& _path) {
    DWORD handle;
    DWORD size = GetFileVersionInfoSizeA(_path.c_str(), &handle);
    if (0 == size) return std::vector<uint_32>();
    std::vector<BYTE> buffer(size, 0X00U);
    if (!GetFileVersionInfoA(_path.c_str(), handle, size, buffer.data())) return std::vector<uint_32>();
    VS_FIXEDFILEINFO* version_info{nullptr};
    UINT len;
    if (!VerQueryValueA(buffer.data(), "\\", (LPVOID*) &version_info, &len)) return std::vector<uint_32>();
    return std::vector<uint_32>{
        (version_info->dwFileVersionMS >> 16) & 0XFFFF,
        (version_info->dwFileVersionMS >> 0) & 0XFFFF,
        (version_info->dwFileVersionLS >> 16) & 0XFFFF,
        (version_info->dwFileVersionLS >> 0) & 0XFFFF};
}

bytes alx::load_resource_data(uint_32 _id) {
    HRSRC h_res = FindResource(nullptr, MAKEINTRESOURCE(_id), RT_RCDATA);
    if (nullptr == h_res) return bytes();
    HGLOBAL h_res_load = LoadResource(NULL, h_res);
    if (nullptr == h_res_load) return bytes();
    DWORD res_size = SizeofResource(NULL, h_res);
    if (0 == res_size) return bytes();
    void* p_res_data = LockResource(h_res_load);
    if (nullptr == p_res_data) return bytes();
    return bytes(p_res_data, res_size);
}

#    include <dwmapi.h>
#    pragma comment(lib, "dwmapi.lib")
HBITMAP alx::snap_process_window(uint_32 _pid) {
    DWORD params[2] = {_pid, 0};
    EnumWindows([](HWND _hwnd, LPARAM _lparam) -> BOOL {
        DWORD* params = reinterpret_cast<DWORD*>(_lparam);
        DWORD targetPid = params[0];
        DWORD windowPid;
        GetWindowThreadProcessId(_hwnd, &windowPid);
        // FALSE stops the walk: the first visible window of that pid is the one kept
        return windowPid == targetPid && IsWindowVisible(_hwnd) ? params[1] = DWORD(reinterpret_cast<uint_64>(_hwnd)), FALSE : TRUE;
    },
                (LPARAM) params);

    HWND hwnd = reinterpret_cast<HWND>(uint_64(0) | params[1]);
    if (!hwnd) return nullptr;

    RECT rc_window;
    GetWindowRect(hwnd, &rc_window);
    int width = rc_window.right - rc_window.left;
    int height = rc_window.bottom - rc_window.top;
    if (width <= 0 || height <= 0) return nullptr;

    HDC hScreenDC = GetDC(NULL);
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);

    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, width, height);
    HBITMAP hOldBitmap = (HBITMAP) SelectObject(hMemoryDC, hBitmap);

    // only the success of the query matters: it decides between copying the window DC and PrintWindow
    HRESULT hr = DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &rc_window, sizeof(RECT));
    if (SUCCEEDED(hr)) {
        HDC hWindowDC = GetDC(hwnd);
        BitBlt(hMemoryDC, 0, 0, width, height, hWindowDC, 0, 0, SRCCOPY);
        ReleaseDC(hwnd, hWindowDC);
    } else PrintWindow(hwnd, hMemoryDC, PW_CLIENTONLY);

    SelectObject(hMemoryDC, hOldBitmap);
    DeleteDC(hMemoryDC);
    ReleaseDC(NULL, hScreenDC);

    return hBitmap;
}
#endif

alx::process_info alx::process_info::current() {
#ifdef _WIN32
    uint_32 self = (uint_32) GetCurrentProcessId();
#else
    uint_32 self = (uint_32)::getpid();
#endif
    std::vector<process_info> list = read_process(self);
    return list.empty() ? process_info() : list.front();
}

std::vector<process_info> alx::process_info::find_process(uint_32 _pid) {
    return read_process(_pid);
}

std::vector<process_info> alx::process_info::find_process(const std::string& _name) {
    if (_name.empty()) return find_process();

    bool full = std::string::npos != _name.find_first_of("\\/");
    return find_process([_name, full](const process_info& _info) -> bool { return full ? _name == _info.path : _name == base_name(_info.path); });
}

std::vector<process_info> alx::process_info::find_process(std::function<bool(const process_info&)> _hit) {
    std::vector<process_info> result;

#ifdef _WIN32
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
    if (hSnapshot == INVALID_HANDLE_VALUE) return result;
    PROCESSENTRY32A pe;
    pe.dwSize = sizeof(PROCESSENTRY32A);

    if (Process32FirstA(hSnapshot, &pe) == FALSE) goto RET;
    do {
        process_info info(pe.th32ProcessID, pe.th32ParentProcessID, pe.cntThreads, pe.pcPriClassBase, process_path(pe.th32ProcessID));
        if (!_hit || _hit(info)) result.push_back(std::move(info));
    } while (Process32NextA(hSnapshot, &pe));

RET:
    CloseHandle(hSnapshot);
    return result;
#else
    DIR* proc_dir = opendir("/proc");
    if (!proc_dir) return result;

    struct dirent* entry;
    while ((entry = readdir(proc_dir)) != nullptr) {
        char* endptr;
        long pid = strtol(entry->d_name, &endptr, 10);
        // an all-digit name too long for a pid overflows strtol to LONG_MAX, which the range test drops
        if (*endptr != '\0' || pid <= 0 || pid > (long) uint_32_npos) continue;

        std::vector<process_info> list = read_process((uint_32) pid);
        if (list.empty()) continue;
        if (!_hit || _hit(list.front())) result.push_back(std::move(list.front()));
    }
    closedir(proc_dir);
    return result;
#endif
}

std::vector<process_info> alx::process_info::find_child_process(uint_32 _ppid) {
    return find_process([_ppid](const process_info& _info) -> bool { return _ppid == _info.ppid; });
}

std::string alx::process_info::base_name(const std::string& _path) {
#ifdef _WIN32
    size_t pos = _path.find_last_of("\\/");
#else
    size_t pos = _path.find_last_of('/');
#endif
    return std::string::npos == pos ? _path : _path.substr(pos + 1);
}

#ifdef _WIN32
std::string alx::process_info::process_path(uint_32 _pid) {
    HANDLE h_process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD) _pid);
    if (nullptr == h_process) return std::string();

    char buffer[4096];
    DWORD size = sizeof(buffer);
    std::string result;
    if (QueryFullProcessImageNameA(h_process, 0, buffer, &size)) result.assign(buffer, size);
    CloseHandle(h_process);
    return result;
}
#else
std::string alx::process_info::process_path(uint_32 _pid) {
    std::string link = "/proc/" + std::to_string(_pid) + "/exe";
    char buffer[4096];
    ssize_t size = ::readlink(link.c_str(), buffer, sizeof(buffer));
    // readlink adds no terminator, and a full buffer means the path was truncated
    if (size <= 0 || (ssize_t) sizeof(buffer) == size) return std::string();
    return std::string(buffer, (size_t) size);
}

int_32 alx::process_info::parse_int(const std::string& _str) {
    return (int_32) strtol(_str.c_str(), nullptr, 10);
}
#endif

std::vector<process_info> alx::process_info::read_process(uint_32 _pid) {
    std::vector<process_info> result;

#ifdef _WIN32
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
    if (hSnapshot == INVALID_HANDLE_VALUE) return result;
    PROCESSENTRY32A pe;
    pe.dwSize = sizeof(PROCESSENTRY32A);

    if (Process32FirstA(hSnapshot, &pe) == FALSE) goto RET;
    do {
        if (pe.th32ProcessID != (DWORD) _pid) continue;
        result.push_back(process_info(pe.th32ProcessID, pe.th32ParentProcessID, pe.cntThreads, pe.pcPriClassBase, process_path(pe.th32ProcessID)));
        break;
    } while (Process32NextA(hSnapshot, &pe));

RET:
    CloseHandle(hSnapshot);
#else
    std::ifstream stat_file("/proc/" + std::to_string(_pid) + "/stat");
    std::string content((std::istreambuf_iterator<char>(stat_file)), std::istreambuf_iterator<char>());

    // comm may hold spaces, brackets and newlines, so the whole file is read and the last ')' anchors the parse
    size_t pos = content.rfind(')');
    if (std::string::npos == pos) return result;

    std::istringstream iss(content.substr(pos + 2));
    std::string state, ppid_str, pgrp_str, session_str, tty_nr_str, tpgid_str, flags_str,
        minflt_str, cminflt_str, majflt_str, cmajflt_str, utime_str, stime_str, cutime_str,
        cstime_str, priority_str, nice_str, thread_cnt_str;
    if (!(iss >> state >> ppid_str >> pgrp_str >> session_str >> tty_nr_str >> tpgid_str >> flags_str >> minflt_str >> cminflt_str >> majflt_str >> cmajflt_str >> utime_str >> stime_str >> cutime_str >> cstime_str >> priority_str >> nice_str >> thread_cnt_str)) return result;

    result.push_back(process_info(
        (pid_t) _pid, (pid_t) parse_int(ppid_str), (size_t) parse_int(thread_cnt_str), parse_int(priority_str), process_path(_pid)));
#endif
    return result;
}

#ifdef _WIN32
alx::process_info::process_info(
    DWORD _pid, DWORD _ppid, DWORD _t_cnt, LONG _b_pri, const std::string& _path)
    : pid(_pid), ppid(_ppid), thread_cnt(_t_cnt), base_priority(_b_pri), path(_path) {
}
#else
alx::process_info::process_info(
    pid_t _pid, pid_t _ppid, size_t _t_cnt, int _b_pri, const std::string& _path)
    : pid(_pid), ppid(_ppid), thread_cnt(_t_cnt), base_priority(_b_pri), path(_path) {
}
#endif

#ifdef _WIN32
bool alx::process_ctrl::start(const std::vector<std::string>& _args, const process_limits& _limits) {
    if (0 != p_id) kill();
    p_status = wait_status();

    std::string command = "\"" + p_exec + "\"";
    for (const auto& arg : _args) command += " \"" + arg + "\"";
    if (command.empty()) return false;

    // the ERR path passes every handle again, so this nulls what it closed
    auto close_handle = [](HANDLE& _handle) {
        if (nullptr != _handle) {
            CloseHandle(_handle);
            _handle = nullptr;
        }
    };

    if (0 != _limits.mem_bytes || _limits.kill_on_parent_exit) {
        h_job = CreateJobObjectA(nullptr, nullptr);
        if (nullptr == h_job) return false;

        JOBOBJECT_EXTENDED_LIMIT_INFORMATION job_info = {};
        if (0 != _limits.mem_bytes) {
            job_info.ProcessMemoryLimit = (SIZE_T) _limits.mem_bytes;
            job_info.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_PROCESS_MEMORY;
        }
        if (_limits.kill_on_parent_exit) job_info.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!SetInformationJobObject(h_job, JobObjectExtendedLimitInformation, &job_info, sizeof(job_info))) {
            close_handle(h_job);
            return false;
        }
    }

    SECURITY_ATTRIBUTES sa;
    DWORD mode = PIPE_READMODE_BYTE | PIPE_NOWAIT;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof(pi));

    if (!CreatePipe(&p_stdin.read, &p_stdin.write, &sa, 0)) goto ERR;
    if (!CreatePipe(&p_stdout.read, &p_stdout.write, &sa, 0)) goto ERR;
    if (!CreatePipe(&p_stderr.read, &p_stderr.write, &sa, 0)) goto ERR;

    if (!SetNamedPipeHandleState(p_stdout.read, &mode, nullptr, nullptr)) goto ERR;
    if (!SetNamedPipeHandleState(p_stderr.read, &mode, nullptr, nullptr)) goto ERR;

    STARTUPINFOA si;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.hStdInput = p_stdin.read;
    si.hStdOutput = p_stdout.write;
    si.hStdError = p_stderr.write;
    si.dwFlags |= STARTF_USESTDHANDLES;

    if (!CreateProcessA(
            nullptr,
            &command[0],
            nullptr,
            nullptr,
            TRUE,
            (nullptr != h_job) ? CREATE_SUSPENDED : 0,
            nullptr,
            nullptr,
            &si,
            &pi)) goto ERR;

    p_id = pi.dwProcessId;
    h_process = pi.hProcess;
    // the child is still suspended: it must never run outside the job
    if (nullptr != h_job) {
        if (!AssignProcessToJobObject(h_job, h_process)) {
            TerminateProcess(h_process, 1);
            goto ERR;
        }
        ResumeThread(pi.hThread);
    }
    close_handle(pi.hThread);
    pi.hProcess = nullptr;

    close_handle(p_stdin.read);
    close_handle(p_stdout.write);
    close_handle(p_stderr.write);
    p_output_done = false;
    t_loop = new thread(std::bind(&process_ctrl::loop, this));
    return true;
ERR:
    close_handle(p_stdin.read);
    close_handle(p_stdin.write);
    close_handle(p_stdout.read);
    close_handle(p_stdout.write);
    close_handle(p_stderr.read);
    close_handle(p_stderr.write);
    close_handle(pi.hThread);
    close_handle(pi.hProcess);
    close_handle(h_job);
    p_id = 0;
    h_process = nullptr;
    return false;
}
#else
bool alx::process_ctrl::start(const std::vector<std::string>& _args, const process_limits& _limits) {
    if (0 != p_id) kill();
    p_status = wait_status();

    if (p_exec.empty()) return false;

    // argv is built before fork: only async-signal-safe calls may run between fork and execv
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>(p_exec.data()));
    for (const auto& args : _args) argv.push_back(const_cast<char*>(args.data()));
    argv.push_back(nullptr);
    const pid_t parent_pid = ::getpid();
    pid_t pid = -1;

    // both ends are CLOEXEC and the parent drops its copy, so EOF on it means the child reached execv
    int status_pipe[2] = {-1, -1};
    if (::pipe(status_pipe) == -1) return false;
    ::fcntl(status_pipe[0], F_SETFD, FD_CLOEXEC);
    ::fcntl(status_pipe[1], F_SETFD, FD_CLOEXEC);

    if (::pipe(p_stdin.pipefd) == -1) goto ERR;
    if (::pipe(p_stdout.pipefd) == -1) goto ERR;
    if (::pipe(p_stderr.pipefd) == -1) goto ERR;

    pid = ::fork();
    if (pid == 0) {
        auto fail = [&status_pipe](int _excode) {
            int error = errno;
            ssize_t ignored = ::write(status_pipe[1], &error, sizeof(error));
            (void) ignored;
            _exit(_excode);
        };

        ::close(status_pipe[0]);
        ::dup2(p_stdin.pipefd[0], STDIN_FILENO);
        ::close(p_stdin.pipefd[0]);
        ::close(p_stdin.pipefd[1]);
        ::dup2(p_stdout.pipefd[1], STDOUT_FILENO);
        ::close(p_stdout.pipefd[0]);
        ::close(p_stdout.pipefd[1]);
        ::dup2(p_stderr.pipefd[1], STDERR_FILENO);
        ::close(p_stderr.pipefd[0]);
        ::close(p_stderr.pipefd[1]);

        if (_limits.kill_on_parent_exit) {
#    ifdef PR_SET_PDEATHSIG
            if (0 != ::prctl(PR_SET_PDEATHSIG, SIGKILL)) fail(126);
            if (::getppid() != parent_pid) _exit(126);
#    else
            fail(126);
#    endif
        }
        if (0 != _limits.mem_bytes) {
            struct rlimit mem_limit;
            mem_limit.rlim_cur = (rlim_t) _limits.mem_bytes;
            // the hard limit is lowered too, so the child cannot raise it back up
            mem_limit.rlim_max = (rlim_t) _limits.mem_bytes;
            if (0 != ::setrlimit(RLIMIT_AS, &mem_limit)) fail(126);
        }

        ::execv(p_exec.c_str(), argv.data());
        fail(127);
    } else if (pid > 0) {
        p_id = pid;
        h_process = pid;
        ::close(p_stdin.pipefd[0]);
        p_stdin.pipefd[0] = -1;
        ::close(p_stdout.pipefd[1]);
        p_stdout.pipefd[1] = -1;
        ::close(p_stderr.pipefd[1]);
        p_stderr.pipefd[1] = -1;
        ::close(status_pipe[1]);
        status_pipe[1] = -1;

        int error = 0;
        ssize_t read_size = 0;
        do {
            read_size = ::read(status_pipe[0], &error, sizeof(error));
        } while (-1 == read_size && EINTR == errno);
        ::close(status_pipe[0]);
        status_pipe[0] = -1;

        if (0 != read_size) {
            int status_code = 0;
            while (-1 == waitpid(pid, &status_code, 0) && EINTR == errno) {
            }
            goto ERR;
        }

        p_output_done = false;
        t_loop = new thread(std::bind(&process_ctrl::loop, this));
        return true;
    } else goto ERR;
ERR:
    if (-1 != status_pipe[0]) {
        ::close(status_pipe[0]);
        status_pipe[0] = -1;
    }
    if (-1 != status_pipe[1]) {
        ::close(status_pipe[1]);
        status_pipe[1] = -1;
    }
    if (p_stdin.pipefd[0] != -1) {
        ::close(p_stdin.pipefd[0]);
        p_stdin.pipefd[0] = -1;
    }
    if (p_stdin.pipefd[1] != -1) {
        ::close(p_stdin.pipefd[1]);
        p_stdin.pipefd[1] = -1;
    }
    if (p_stdout.pipefd[0] != -1) {
        ::close(p_stdout.pipefd[0]);
        p_stdout.pipefd[0] = -1;
    }
    if (p_stdout.pipefd[1] != -1) {
        ::close(p_stdout.pipefd[1]);
        p_stdout.pipefd[1] = -1;
    }
    if (p_stderr.pipefd[0] != -1) {
        ::close(p_stderr.pipefd[0]);
        p_stderr.pipefd[0] = -1;
    }
    if (p_stderr.pipefd[1] != -1) {
        ::close(p_stderr.pipefd[1]);
        p_stderr.pipefd[1] = -1;
    }

    p_id = 0;
    h_process = -1;
    return false;
}
#endif

alx::process_ctrl::~process_ctrl() {
    kill();
}

bool alx::process_ctrl::input(const std::string& _string) {

    // writing into a dead child's pipe would raise SIGPIPE on POSIX: liveness first
    if (0 == p_id || status().exited) return false;
#ifdef _WIN32
    DWORD written{0};
    return nullptr != p_stdin.write &&
           WriteFile(p_stdin.write, _string.c_str(), (DWORD) _string.size(), &written, nullptr) &&
           written == _string.size();
#else
    if (p_stdin.pipefd[1] == -1) return false;
    ptrdiff_t written = ::write(p_stdin.pipefd[1], _string.c_str(), _string.size());
    return (size_t) (written) == _string.size();
#endif
}

bool alx::process_ctrl::terminate() {
    if (0 == p_id || status().exited) return false;

#ifdef _WIN32
    if (nullptr == p_stdin.write) return false;
    CloseHandle(p_stdin.write);
    p_stdin.write = nullptr;
    return true;
#else
    return 0 == ::kill(p_id, SIGTERM);
#endif
}

bool alx::process_ctrl::kill() {
    if (0 == p_id) return false;

    // status() has already settled a child that died, so the kill below runs only on a live one
    bool was_alive = !status().exited;
#ifdef _WIN32
    if (was_alive) {
        TerminateProcess(h_process, 1);
        WaitForSingleObject(h_process, INFINITE);
    }
    DWORD code{0};
    if (GetExitCodeProcess(h_process, &code)) {
        p_status.exited = true;
        p_status.exit_code = (int_32) code;
    }
    if (nullptr != h_job) {
        CloseHandle(h_job);
        h_job = nullptr;
    }
    // the reader loop spins on p_id: clearing it is what lets the join below return
    p_id = 0;
    if (nullptr != t_loop) {
        t_loop->join();
        delete t_loop;
        t_loop = nullptr;
    }
    if (nullptr != h_process) {
        CloseHandle(h_process);
        h_process = nullptr;
    }
    if (nullptr != p_stdin.write) {
        CloseHandle(p_stdin.write);
        p_stdin.write = nullptr;
    }
    if (nullptr != p_stdout.read) {
        CloseHandle(p_stdout.read);
        p_stdout.read = nullptr;
    }
    if (nullptr != p_stderr.read) {
        CloseHandle(p_stderr.read);
        p_stderr.read = nullptr;
    }
#else
    if (was_alive) {
        ::kill(p_id, SIGKILL);
        int status_code{0};
        pid_t reaped = -1;
        while (-1 == (reaped = waitpid(p_id, &status_code, 0)) && EINTR == errno) {
        }
        if ((pid_t) p_id == reaped) p_status = decode_wait_status(status_code);
    }
    p_id = 0;
    h_process = -1;
    if (nullptr != t_loop) {
        t_loop->join();
        delete t_loop;
        t_loop = nullptr;
    }
    if (p_stdin.pipefd[1] != -1) {
        ::close(p_stdin.pipefd[1]);
        p_stdin.pipefd[1] = -1;
    }
    if (p_stdout.pipefd[0] != -1) {
        ::close(p_stdout.pipefd[0]);
        p_stdout.pipefd[0] = -1;
    }
    if (p_stderr.pipefd[0] != -1) {
        ::close(p_stderr.pipefd[0]);
        p_stderr.pipefd[0] = -1;
    }
#endif
    p_output_done = true;
    return was_alive;
}

alx::process_ctrl::wait_status alx::process_ctrl::status() {
    if (0 == p_id || p_status.exited) return p_status;

#ifdef _WIN32
    DWORD code{0};
    if (GetExitCodeProcess(h_process, &code) && STILL_ACTIVE != code) {
        p_status.exited = true;
        p_status.exit_code = (int_32) code;
    }
#else
    int status_code{0};
    if ((pid_t) p_id == waitpid(p_id, &status_code, WNOHANG)) p_status = decode_wait_status(status_code);
#endif
    return p_status;
}

bool alx::process_ctrl::wait(const uint_32 _mill) {
    uint_32 waited{0};
    while (0 != p_id && !status().exited && waited < _mill) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        waited += 5;
    }
    return status().exited;
}

bool alx::process_ctrl::output_done(const uint_32 _mill) {
    uint_32 waited{0};
    while (!p_output_done.load() && waited < _mill) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        waited += 5;
    }
    return p_output_done.load();
}

void alx::process_ctrl::loop() {
    constexpr size_t buffer_size = 4096;
    bool has_data{false};
    char buffer[buffer_size];
#ifdef _WIN32
    DWORD bytes_read{0};
    bool stdout_eof = false;
    bool stderr_eof = false;
    while (0 != p_id) {
        has_data = false;
        if (!stdout_eof && nullptr != p_stdout.read) {
            if (0 == ReadFile(p_stdout.read, buffer, buffer_size, &bytes_read, nullptr)) {
                DWORD error = GetLastError();
                stdout_eof = ERROR_BROKEN_PIPE == error || ERROR_HANDLE_EOF == error;
            } else if (0 != bytes_read) {
                sig_output.exec(string(buffer, bytes_read));
                has_data = true;
            } else stdout_eof = true;
        }
        if (!stderr_eof && nullptr != p_stderr.read) {
            if (0 == ReadFile(p_stderr.read, buffer, buffer_size, &bytes_read, nullptr)) {
                DWORD error = GetLastError();
                stderr_eof = ERROR_BROKEN_PIPE == error || ERROR_HANDLE_EOF == error;
            } else if (0 != bytes_read) {
                sig_errput.exec(string(buffer, bytes_read));
                has_data = true;
            } else stderr_eof = true;
        }
        if (stdout_eof && stderr_eof) {
            p_output_done = true;
            break;
        }
        if (!has_data) Sleep(15);
    }
#else
    ptrdiff_t bytes_read{0};
    bool stdout_eof = false;
    bool stderr_eof = false;
    while (0 != p_id) {
        has_data = false;
        if (!stdout_eof && p_stdout.pipefd[0] != -1) {
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(p_stdout.pipefd[0], &read_fds);

            struct timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 10000;

            int ret = ::select(p_stdout.pipefd[0] + 1, &read_fds, nullptr, nullptr, &tv);

            if (ret > 0 && FD_ISSET(p_stdout.pipefd[0], &read_fds)) {
                bytes_read = read(p_stdout.pipefd[0], buffer, buffer_size);
                if (bytes_read > 0) {
                    sig_output.exec(std::string(buffer, bytes_read));
                    has_data = true;
                } else if (bytes_read == 0) stdout_eof = true;
            }
        }
        if (!stderr_eof && p_stderr.pipefd[0] != -1) {
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(p_stderr.pipefd[0], &read_fds);

            struct timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 10000;

            int ret = select(p_stderr.pipefd[0] + 1, &read_fds, nullptr, nullptr, &tv);

            if (ret > 0 && FD_ISSET(p_stderr.pipefd[0], &read_fds)) {
                bytes_read = read(p_stderr.pipefd[0], buffer, buffer_size);
                if (bytes_read > 0) {
                    sig_errput.exec(std::string(buffer, bytes_read));
                    has_data = true;
                } else if (bytes_read == 0) stderr_eof = true;
            }
        }
        if (stdout_eof && stderr_eof) {
            p_output_done = true;
            break;
        }
        if (!has_data) std::this_thread::sleep_for(std::chrono::milliseconds(15));
    }
#endif
}

#ifndef _WIN32
alx::process_ctrl::wait_status alx::process_ctrl::decode_wait_status(int _status) {
    wait_status result;
    result.exited = true;
    result.exit_code = WIFEXITED(_status) ? WEXITSTATUS(_status) : -1;
    result.signaled = WIFSIGNALED(_status);
    result.term_sig = WIFSIGNALED(_status) ? WTERMSIG(_status) : 0;
    return result;
}
#endif
