// Copyright (c) 2026 AlexisVon

#include "afile.h"
#include "ascript.h"

using namespace alx;
using namespace alx::script;

namespace {
    inline void check_str_arg(fwrap& args, size_t _i, const char* _fn) {
        if (_i >= args.size() || !args[_i].is<std::string>())
            args.raise(variant(std::string(_fn) + ": expected a path string"), error_type::ArgError);
    }
}

static void fn_mkdir(fwrap& args) {
    check_str_arg(args, 0, "mkdir");
    args.freturn(file_info::mkdir(args[0].to<std::string>()));
}

static void fn_rmfile(fwrap& args) {
    check_str_arg(args, 0, "rmfile");
    args.freturn(file_info::rmfile(args[0].to<std::string>()));
}

static void fn_rmdir(fwrap& args) {
    check_str_arg(args, 0, "rmdir");
    args.freturn(file_info::rmdir(args[0].to<std::string>()));
}

static void fn_mvfile(fwrap& args) {
    check_str_arg(args, 0, "mvfile");
    check_str_arg(args, 1, "mvfile");
    args.freturn(file_info::mvfile(args[0].to<std::string>(), args[1].to<std::string>()));
}

static void fn_read(fwrap& args) {
    alx::file* f = args.unwrap<alx::file>();
    if (!f)
        args.raise(variant(std::string("file: not initialized")), error_type::RuntimeError);
    if (!f->is_read_able())
        args.raise(variant(std::string("file.read: file not opened for reading")), error_type::RuntimeError);

    uint_64 pos = 0;
    uint_64 size = uint_64(-1);
    if (args.size() > 0) {
        if (!args[0].is<int_64>())
            args.raise(variant(std::string("read: pos must be integer")), error_type::TypeError);
        pos = static_cast<uint_64>(args[0].to<int_64>());
    }
    if (args.size() > 1) {
        if (!args[1].is<int_64>())
            args.raise(variant(std::string("read: size must be integer")), error_type::TypeError);
        size = static_cast<uint_64>(args[1].to<int_64>());
    }

    bytes b = f->read(pos, size);
    args.freturn(std::string(reinterpret_cast<const char*>(b.data()), b.size()));
}

static void fn_write(fwrap& args) {
    alx::file* f = args.unwrap<alx::file>();
    if (!f)
        args.raise(variant(std::string("file: not initialized")), error_type::RuntimeError);
    if (!f->is_write_able())
        args.raise(variant(std::string("file.write: file not opened for writing")), error_type::RuntimeError);

    if (args.size() < 1 || !args[0].is<std::string>())
        args.raise(variant(std::string("write: expected (data, pos?)")), error_type::ArgError);

    const std::string& data = args[0].to<std::string>();
    uint_64 pos = uint_64(-1);
    if (args.size() > 1) {
        if (!args[1].is<int_64>())
            args.raise(variant(std::string("write: pos must be integer")), error_type::TypeError);
        pos = static_cast<uint_64>(args[1].to<int_64>());
    }
    args.freturn(f->write(data.data(), data.size(), pos));
}

static void fn_flush(fwrap& args) {
    alx::file* f = args.unwrap<alx::file>();
    if (f) f->flush();
}

static void fn_close(fwrap& args) {
    alx::file* f = args.unwrap<alx::file>();
    if (f) f->close();
}

static void fn_is_open(fwrap& args) {
    alx::file* f = args.unwrap<alx::file>();
    args.freturn(f ? f->is_open() : false);
}

static void fn_open(fwrap& args) {
    if (args.size() < 3)
        args.raise(variant(std::string("fs.open: expected (name, path, mode)")), error_type::ArgError);
    if (!args[0].is<std::string>() || !args[1].is<std::string>() || !args[2].is<std::string>())
        args.raise(variant(std::string("fs.open: name, path and mode must be strings")), error_type::TypeError);

    std::string name = args[0].to<std::string>();
    std::string path = args[1].to<std::string>();
    std::string mode = args[2].to<std::string>();

    uint_8 io_type;
    if (mode == "r") io_type = alx::file::READ;
    else if (mode == "w") io_type = alx::file::WRIT;
    else if (mode == "a") io_type = alx::file::APED;
    else if (mode == "rw") io_type = alx::file::R__W;
    else {
        args.raise(variant(std::string("fs.open: invalid mode '") + mode + "', expected r|w|a|rw"),
                   error_type::ArgError);
        return;
    }

    alx::file* f = new alx::file();
    if (!f->open(file_info(path), io_type)) {
        delete f;
        args.raise(variant(std::string("fs.open: cannot open '") + path + "'"), error_type::RuntimeError);
        return;
    }

    args.remove(name);
    args.wrap<alx::file>(f, name, {
                                      {"read", fn_read},
                                      {"write", fn_write},
                                      {"flush", fn_flush},
                                      {"close", fn_close},
                                      {"is_open", fn_is_open},
                                  });
    args.freturn(true);
}

static void fn_fi_path(fwrap& args) {
    file_info* fi = args.unwrap<file_info>();
    if (fi) args.freturn(variant(fi->path()));
}

static void fn_fi_name(fwrap& args) {
    file_info* fi = args.unwrap<file_info>();
    if (fi) args.freturn(variant(fi->name()));
}

static void fn_fi_suffix(fwrap& args) {
    file_info* fi = args.unwrap<file_info>();
    if (fi) args.freturn(variant(fi->suffix()));
}

static void fn_fi_size(fwrap& args) {
    file_info* fi = args.unwrap<file_info>();
    if (fi) args.freturn(variant(static_cast<int_64>(fi->size())));
}

static void fn_fi_is_dir(fwrap& args) {
    file_info* fi = args.unwrap<file_info>();
    if (fi) args.freturn(fi->is_dir());
}

static void fn_fi_is_exist(fwrap& args) {
    file_info* fi = args.unwrap<file_info>();
    if (fi) args.freturn(fi->is_exist());
}

static void fn_fi_is_link(fwrap& args) {
    file_info* fi = args.unwrap<file_info>();
    if (fi) args.freturn(fi->is_link());
}

static void fn_fi_get_parent(fwrap& args) {
    file_info* fi = args.unwrap<file_info>();
    if (!fi) return;
    file_info parent = fi->get_parent();
    args.freturn(parent.is_valid() ? variant(parent.path()) : variant(std::string("")));
}

static void fn_fi_get_child(fwrap& args) {
    file_info* fi = args.unwrap<file_info>();
    if (!fi) return;
    std::string filter = (args.size() > 0 && args[0].is<std::string>()) ? args[0].to<std::string>() : "*";
    auto children = fi->get_child(filter);
    varvec result;
    for (auto& c : children) result.push_back(variant(c.path()));
    args.freturn(std::move(result));
}

static void fn_info(fwrap& args) {
    if (args.size() < 2)
        args.raise(variant(std::string("fs.info: expected (name, path)")), error_type::ArgError);
    if (!args[0].is<std::string>() || !args[1].is<std::string>())
        args.raise(variant(std::string("fs.info: name and path must be strings")), error_type::TypeError);

    std::string name = args[0].to<std::string>();
    file_info* fi = new file_info(args[1].to<std::string>());

    args.remove(name);
    args.wrap<file_info>(fi, name, {
                                       {"path", fn_fi_path},
                                       {"name", fn_fi_name},
                                       {"suffix", fn_fi_suffix},
                                       {"size", fn_fi_size},
                                       {"is_dir", fn_fi_is_dir},
                                       {"is_exist", fn_fi_is_exist},
                                       {"is_link", fn_fi_is_link},
                                       {"get_parent", fn_fi_get_parent},
                                       {"get_child", fn_fi_get_child},
                                   });
    args.freturn(true);
}

void register_fs(fwrap& args) {

    args.bind("mkdir", fn_mkdir, "fs");
    args.bind("rmfile", fn_rmfile, "fs");
    args.bind("rmdir", fn_rmdir, "fs");
    args.bind("mvfile", fn_mvfile, "fs");

    args.bind("open", fn_open, "fs");
    args.bind("info", fn_info, "fs");
}
