/*****************************************************************/ /**
 * \file   main.cpp
 * \brief  Alexis Script CLI — run / compile / dump / REPL
 *
 * Usage: Scpt [file.axc] [-e "code"] [-c] [-j] [-x file.axc] [-i]
 *
 * \author alexis
 * Copyright (c) 2026 AlexisVon
 ******************************************************************************/

#include "acompress.h"
#include "adatetime.h"
#include "afile.h"
#include "ajson.h"
#include "aplatform.h"
#include "ascript.h"
#include "astring.h"
#include "avarsolid.h"
#ifndef _WIN32
#    include "line_editor.h"
#endif
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#ifdef _WIN32
#    include <windows.h>
#else
#    include <unistd.h>
#endif

using namespace alx;

static bool to_bool_strict(const variant& _v) {
    if (_v.is<bool>()) return _v.to<bool>();
    if (_v.is<int_64>()) return _v.to<int_64>() != 0;
    return false;
}

static void print_usage() {
    std::cout
        << "Alexis Script (example-0.1)\n"
        << "Usage: Scpt [options] [file.axc]\n"
        << "  file.axc       Execute script file\n"
        << "  -e \"code\"      Execute inline code\n"
        << "  -c             Compile file.axc to file.axp\n"
        << "  -o out.axp    Output path for -c (default: file.axp)\n"
        << "  -j             Print compiled AST (human-readable)\n"
        << "  -x file.axp   Execute compiled binary\n"
        << "  -p path        Add search path for import/link\n"
        << "  --overflow-check  Enable integer overflow/shift checking\n"
        << "  --max-stack N     Set script frame depth limit (default 1024, 0=unlimited)\n"
        << "  --max-vecfill N   Set max vec fill count (default 4096, 0=unlimited)\n"
        << "  --embed        Self-contained compile (with -c only; link/env forbidden,\n"
        << "                 unresolved import = error)\n"
        << "  -i             Interactive REPL mode\n"
        << "  -v [ver]       Show or set engine version\n"
        << "  -h             Show this help\n";
}

static void write_file(const std::string& _path, const bytes& _data) {
    std::ofstream f(_path, std::ios::binary);
    if (f) f.write(reinterpret_cast<const char*>(_data.data()), _data.size());
}

static void ext_fread(script::fwrap& fw) {
    std::string path = fw[0].to<std::string>();
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        fw.raise(variant("fread: cannot open file: " + path));
        return;
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    fw.freturn(variant(ss.str()));
}

static void ext_fwrite(script::fwrap& fw) {
    std::string path = fw[0].to<std::string>();
    std::string content = fw[1].to<std::string>();
    std::ofstream f(path, std::ios::binary);
    if (!f) {
        fw.raise(variant("fwrite: cannot open file: " + path));
        return;
    }
    f.write(content.data(), content.size());
    fw.freturn(variant(f.good()));
}

static void ext_exec(script::fwrap& fw) {
    std::string cmd = fw[0].to<std::string>();
    uint_32 timeout = fw.size() > 1 ? static_cast<uint_32>(fw[1].to<int_64>(5000)) : 5000;
    auto r = alx::exec_sync(cmd, timeout);
    varmap result;
    result["output"] = variant(r.output);
    result["excode"] = variant(static_cast<int_64>(r.excode));
    result["success"] = variant(r.success);
    fw.freturn(variant(std::move(result)));
}

static void ext_print(script::fwrap& fw) {
    std::vector<std::string> args;
    for (size_t i = 0; i < fw.size(); i++) {
        const variant& v = fw[i];
        switch (v.type()) {
        case variant::id<std::string>(): args.push_back(v.to<std::string>()); break;
        case variant::id<int_64>(): args.push_back(std::to_string(v.to<int_64>())); break;
        case variant::id<double>(): args.push_back(std::to_string(v.to<double>())); break;
        case variant::id<bool>(): args.push_back(v.to<bool>() ? "true" : "false"); break;
        case variant::id<bytes>(): args.push_back(strutil::format("bytes(%1)", std::to_string(v.to<bytes>().size())));
        case variant::id<varvec>(): args.push_back(strutil::format("vec(%1)", std::to_string(v.to<varvec>().size())));
        case variant::id<varlst>(): args.push_back(strutil::format("lst(%1)", std::to_string(v.to<varlst>().size())));
        case variant::id<varmap>(): args.push_back(strutil::format("map(%1)", std::to_string(v.to<varmap>().size())));
        default: args.push_back("null"); break;
        }
    }

    std::string output;
    if (args.size() == 1) {
        output = args[0];
    } else if (args.size() > 1) {
        std::vector<std::string> fmt_args(args.begin() + 1, args.end());
        output = strutil::format(args[0], fmt_args);
    }
    if (!output.empty()) std::cout << output << std::flush;
}

static void ext_input(script::fwrap& fw) {
    std::string line;
    if (!std::getline(std::cin, line)) line = "";
    fw.freturn(variant(std::move(line)));
}

static void ext_tojs(script::fwrap& fw) {
    if (fw.size() < 1) {
        fw.freturn(variant(std::string("null")));
        return;
    }
    const variant& v = fw[0];
    bool compact = fw.size() > 1 && to_bool_strict(fw[1]);
    fw.freturn(variant(json_doc::to_string(json_value::from_variant(v), compact)));
}

static void ext_datetime(script::fwrap& fw) {
    datetime dt = datetime::current();
    varmap m;
    m["year"] = variant(int_64(dt.year()));
    m["month"] = variant(int_64(dt.month()));
    m["day"] = variant(int_64(dt.day()));
    m["h"] = variant(int_64(dt.hour()));
    m["m"] = variant(int_64(dt.minute()));
    m["s"] = variant(int_64(dt.second()));
    m["ms"] = variant(int_64(dt.millisecond()));
    m["us"] = variant(int_64(dt.microsecond()));
    fw.freturn(variant(m));
}

static void ext_fmjs(script::fwrap& fw) {
    if (fw.size() < 1 || !fw[0].is<std::string>()) {
        fw.freturn();
        return;
    }
    bool ok = false;
    json_value jv = json_doc::from_value(fw[0].to<std::string>(), &ok);
    if (!ok) {
        fw.freturn();
        return;
    }
    fw.freturn(jv.take_variant());
}

static std::string cli_val(const variant& _v) {
    if (_v.is<int_64>()) return std::to_string(_v.to<int_64>());
    if (_v.is<double>()) return std::to_string(_v.to<double>());
    if (_v.is<bool>()) return _v.to<bool>() ? "true" : "false";
    if (_v.is<std::string>()) return "\"" + _v.to<std::string>() + "\"";
    if (_v.is<varvec>()) return "[vec]";
    if (_v.is<varmap>()) return "{map}";
    if (_v.is<varlst>()) return "[lst]";
    return "null";
}

static bool cli_hook(script::hook_info& info) {
    if (info.type != script::hook_event::trap) return true;
    auto& m = info.info.to<varmap>();
    auto& here = m.value("here").to<varmap>();
    std::string file = here.value("file").to<std::string>();
    if (file.empty()) file = "?";
    std::cout << "[trap] " << file << ":" << here.value("row").to<int_64>()
              << ":" << here.value("col").to<int_64>();
    if (m.contain("args")) std::cout << "  arg=" << cli_val(m.value("args"));
    std::cout << "\n";
    for (uint_64 i = info.wkfm_size(); i > 0; i--) {
        uint_64 idx = i - 1;
        std::cout << "    at " << info.wkfm_func(idx);
        auto keys = info.wkfm_data_keys(idx);
        if (!keys.empty()) {
            std::cout << "  (";
            for (size_t k = 0; k < keys.size(); k++) {
                if (k) std::cout << ", ";
                std::cout << keys[k] << "="
                          << cli_val(*info.wkfm_data_cptr(idx, keys[k].c_str()));
            }
            std::cout << ")";
        }
        std::cout << "\n";
    }
    return true;
}

static std::string s_cli_version = "example-0.1";
static std::list<std::string> s_cli_paths;

static script::engine* make_engine(bool overflow_check = false, size_t max_stack = 0, size_t max_vecfill = 0) {
    auto* eng = script::engine::create();
    eng->set_etype(s_cli_version);
    eng->set_vtype(7);

    eng->set_hook(cli_hook, nullptr, 0);
    if (overflow_check) eng->set_overflow_check(true);
    if (max_stack > 0) eng->set_max_stack(max_stack);
    if (max_vecfill > 0) eng->set_max_vecfill(max_vecfill);

    std::list<std::string> paths;
    {
        char exe[4096] = {};
#ifdef _WIN32
        GetModuleFileNameA(nullptr, exe, sizeof(exe));
#else
        readlink("/proc/self/exe", exe, sizeof(exe));
#endif
        std::string ep(exe);
        auto pos = ep.rfind('/');
        if (pos != std::string::npos) paths.push_back(ep.substr(0, pos));
    }
    for (auto& p : s_cli_paths) paths.push_back(p);
    eng->set_search_paths(paths);
    eng->on_cerr.connect(
        [](const std::string& _s) { std::cerr << "Error: " << _s << "\n"; });
    eng->on_cmpl.connect([](const script::compile_error& _e) {
        if (!_e.loc.path.empty()) std::cerr << _e.loc.path << ":";
        std::cerr << _e.loc.row << ":" << _e.loc.col
                  << ": error: " << _e.msg << "\n";
    });

    auto register_ext = [](script::engine* _e, const char* _name, script::native_func _fn) {
        if (!_e->set_extend(_name, _fn))
            std::cerr << "warning: extension registration failed: $" << _name << "\n";
    };
    register_ext(eng, "fread", ext_fread);
    register_ext(eng, "fwrite", ext_fwrite);
    register_ext(eng, "exec", ext_exec);
    register_ext(eng, "print", ext_print);
    register_ext(eng, "input", ext_input);
    register_ext(eng, "tojs", ext_tojs);
    register_ext(eng, "fmjs", ext_fmjs);
    register_ext(eng, "datetime", ext_datetime);

    if (!eng->set_define("PI", variant(3.14159)))
        std::cerr << "warning: static definition registration failed: $PI\n";
    if (!eng->set_define("vtype", variant(int_64(4))))
        std::cerr << "warning: static definition registration failed: $vtype\n";
    if (!eng->set_define("etype", variant(std::string(s_cli_version))))
        std::cerr << "warning: static definition registration failed: $etype\n";
    return eng;
}

static void print_result(const variant& _v) {
    if (_v.is<int_64>())
        std::cout << _v.to<int_64>() << "\n";
    else if (_v.is<double>())
        std::cout << _v.to<double>() << "\n";
    else if (_v.is<bool>())
        std::cout << (_v.to<bool>() ? "true" : "false") << "\n";
    else if (_v.is<std::string>())
        std::cout << "\"" << _v.to<std::string>() << "\"\n";
    else if (_v.is<varvec>()) {
        auto& vec = _v.to<varvec>();
        std::cout << "[";
        for (size_t i = 0; i < vec.size(); i++) {
            if (i) std::cout << ", ";
            if (vec[i].is<int_64>()) std::cout << vec[i].to<int_64>();
            else if (vec[i].is<double>()) std::cout << vec[i].to<double>();
            else if (vec[i].is<std::string>())
                std::cout << "\"" << vec[i].to<std::string>() << "\"";
            else if (vec[i].is<bool>())
                std::cout << (vec[i].to<bool>() ? "true" : "false");
            else std::cout << "null";
        }
        std::cout << "]\n";
    } else if (_v.is<varmap>()) {
        std::cout << "{...}\n";
    }
}

static void run_repl() {
    std::cout << "Alexis Script REPL. Type 'exit' to quit.\n";
    script::engine* eng = make_engine();

    std::string line, buffer;
#ifdef _WIN32
    while (true) {
        if (buffer.empty()) std::cout << ">>> " << std::flush;
        else std::cout << "... " << std::flush;
        if (!std::getline(std::cin, line)) break;
        if (line == "exit") break;
#else
    LineEditor editor;
    while (true) {
        if (buffer.empty()) std::cout << ">>> " << std::flush;
        else std::cout << "... " << std::flush;
        if (!editor.readline(line)) break;
        if (line == "exit") break;
#endif
        buffer += line;

        int brace_count = 0;
        for (char c : buffer) {
            if (c == '{') brace_count++;
            if (c == '}') brace_count--;
        }

        if (brace_count <= 0 && !buffer.empty() && (buffer.back() == ';' || buffer.back() == '}')) {
            bytes src(buffer.c_str());
            auto res = eng->exec(bytes_view(src), "");
            print_result(res.value);
            buffer.clear();
        } else {
            buffer += "\n";
        }
    }
    delete eng;
    std::cout << "\n";
}

int main(int _argc, char** _argv) {
    if (_argc < 2) {
        print_usage();
        return 0;
    }

    std::string inline_code, file_path, compile_out, exec_bin;
    bool json_dump = false, interactive = false, do_compile = false;
    bool overflow_check = false, embed_mode = false;
    size_t max_stack = 0;
    size_t max_vecfill = 0;

    for (int i = 1; i < _argc; i++) {
        if (std::strcmp(_argv[i], "-h") == 0) {
            print_usage();
            return 0;
        } else if (std::strcmp(_argv[i], "-e") == 0 && i + 1 < _argc)
            inline_code = _argv[++i];
        else if (std::strcmp(_argv[i], "-c") == 0)
            do_compile = true;
        else if (std::strcmp(_argv[i], "-o") == 0 && i + 1 < _argc)
            compile_out = _argv[++i];
        else if (std::strcmp(_argv[i], "-j") == 0)
            json_dump = true;
        else if (std::strcmp(_argv[i], "-x") == 0 && i + 1 < _argc)
            exec_bin = _argv[++i];
        else if (std::strcmp(_argv[i], "-p") == 0 && i + 1 < _argc)
            s_cli_paths.push_back(_argv[++i]);
        else if (std::strcmp(_argv[i], "--overflow-check") == 0)
            overflow_check = true;
        else if (std::strcmp(_argv[i], "--max-stack") == 0 && i + 1 < _argc)
            max_stack = static_cast<size_t>(std::stoull(_argv[++i]));
        else if (std::strcmp(_argv[i], "--max-vecfill") == 0 && i + 1 < _argc)
            max_vecfill = static_cast<size_t>(std::stoull(_argv[++i]));
        else if (std::strcmp(_argv[i], "--embed") == 0)
            embed_mode = true;
        else if (std::strcmp(_argv[i], "-i") == 0)
            interactive = true;
        else if (std::strcmp(_argv[i], "-v") == 0) {
            if (i + 1 < _argc && _argv[i + 1][0] != '-')
                s_cli_version = _argv[++i];
            else {
                std::cout << s_cli_version << "\n";
                return 0;
            }
        } else if (_argv[i][0] != '-')
            file_path = _argv[i];
    }

    if (!compile_out.empty() && !do_compile && !json_dump) {
        std::cerr << "-o requires -c or -j\n";
        return 1;
    }

    if (embed_mode && !do_compile) {
        std::cerr << "--embed requires -c\n";
        return 1;
    }

    if (!exec_bin.empty()) {
        bytes content = file::read_all(exec_bin);
        if (content.empty()) {
            std::cerr << "Cannot read: " << exec_bin << "\n";
            return 1;
        }
        script::engine* eng = make_engine(overflow_check, max_stack, max_vecfill);
        auto res = eng->exec(bytes_view(content), "");
        delete eng;
        return static_cast<int>(res.error);
    }

    if (json_dump && !do_compile) {
        bytes raw;
        if (!file_path.empty()) raw = file::read_all(file_path);
        else raw = bytes(inline_code.c_str());
        std::string ast = script::engine::prtast(bytes_view(raw));
        if (ast.rfind("Error:", 0) == 0) {
            std::cerr << ast;
            return 1;
        }
        std::cout << ast << std::flush;
        return 0;
    }

    if (do_compile) {
        if (!inline_code.empty() || !file_path.empty()) {
            bytes raw;
            if (!file_path.empty()) raw = file::read_all(file_path);
            else raw = bytes(inline_code.c_str());

            script::engine* eng = make_engine(overflow_check, max_stack, max_vecfill);
            bytes compiled = file_path.empty()
                                 ? eng->compile(bytes_view(raw), "", false, embed_mode)
                                 : eng->compile(file_path, false, embed_mode);
            delete eng;

            if (compiled.empty()) {
                std::cerr << "Compilation failed\n";
                return 1;
            }

            {
                std::string out = compile_out.empty()
                                      ? (file_path.empty() ? "out.axp"
                                                           : file_path.substr(0, file_path.rfind('.')) + ".axp")
                                      : compile_out;
                write_file(out, compiled);
                std::cout << "Compiled: " << out << " (" << compiled.size()
                          << " bytes)\n";
            }
            return 0;
        }
        std::cerr << "Usage: -c/-j requires -e or file\n";
        return 1;
    }

    script::engine* eng = make_engine(overflow_check, max_stack, max_vecfill);
    script::engine::result res;

    if (!inline_code.empty()) {
        bytes src(inline_code.c_str());
        res = eng->exec(bytes_view(src), "");
    }

    if (!file_path.empty()) {

        {
            auto paths = eng->config().search_paths;
            auto pos = file_path.rfind('/');
            if (pos != std::string::npos)
                paths.push_back(file_path.substr(0, pos));
            else
                paths.push_back(".");
            eng->set_search_paths(paths);
        }

        res = eng->exec(file_path);
    }

    int exit_code = static_cast<int>(res.error);

    if (interactive) {
        delete eng;
        run_repl();
    } else {
        delete eng;
    }

    return exit_code;
}
