#include "res.h"
#include "afile.h"
#include "alogger.h"

namespace {
    std::string g_root;

    std::string exe_dir(const char* _argv0) {
        if (nullptr == _argv0 || '\0' == *_argv0) return std::string();
        const std::string path(_argv0);
        const std::size_t pos = path.find_last_of("/\\");
        return std::string::npos == pos ? std::string() : path.substr(0, pos);
    }

    bool is_dir(const std::string& _path) {
        const alx::file_info info(_path);
        return info.is_exist() && info.is_dir();
    }
}

// Resource loading is the only place that knows where page assets live; a Win32
// FindResource backend can replace this file without touching any caller.
void res::init(const char* _argv0) {
    const std::string dir = exe_dir(_argv0);
    const std::string beside_exe = (dir.empty() ? std::string() : dir + "/") + "Hfs.page";
    if (is_dir(beside_exe)) g_root = beside_exe;
    else if (is_dir("./page")) g_root = "./page";
    else if (is_dir("./Hfs.page")) g_root = "./Hfs.page";

    if (g_root.empty()) alx::log_warn().type("res") << "page resources not found";
    else alx::log_info().type("res") << g_root;
}

bool res::load(const std::string& _key, alx::bytes& _out) {
    if (g_root.empty()) return false;
    bool ok{false};
    _out = alx::file::read_all(g_root + "/" + _key, &ok);
    return ok;
}
