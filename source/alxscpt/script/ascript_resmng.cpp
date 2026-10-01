/*****************************************************************/ /**
 * \file   ascript_resmng.cpp
 * \brief  Resource layer implementation — AST cache + link handle pool
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#include "ascript_resmng.h"
#include "afile.h"
#include "averify.h"
#include "avarsolid.h"
#include "script/ascript_compile.h"
#include "script/ascript_fwrap.h"
#include "script/ascript_lex.h"
#include "script/ascript_parse.h"
#include "script/ascript_walk.h"

#if defined(__linux__)
#    include <dlfcn.h>
#elif defined(_WIN32)
#    include <windows.h>
#endif

namespace alx {
    namespace script {

        static bool is_absolute_path(const std::string& _p) {
            if (_p.empty()) return false;
            if (_p[0] == '/') return true;
#if defined(_WIN32)
            if (_p.size() >= 3 && _p[1] == ':' && (_p[2] == '\\' || _p[2] == '/')) return true;
#endif
            return false;
        }

        static std::string join_path(const std::string& _base, const std::string& _rel) {
            if (_base.empty()) return _rel;
            if (_rel.empty()) return _base;
            char last = _base.back();
            std::string sep = (last == '/' || last == '\\') ? "" : "/";
            return _base + sep + _rel;
        }

        static void fire(const alx::signal<uint_64, const std::string&>* _sig,
                         const std::string& _msg) {
            if (!_sig) return;
            _sig->exec(alx::this_tid(), _msg);
        }

        // '@<16 hex>' is already the compile-side module key: decode it back, never hash it
        // plain path: SHA-256 of the path, first 8 bytes; a fresh hasher per call, so no lock
        uint_64 res_make_key(const std::string& _path) {
            if (!_path.empty() && _path[0] == '@')
                return bytes::from_hex(_path.substr(1)).to<uint_64>(0);

            std::unique_ptr<alx::verify> h(alx::verify::create(alx::verify::SHA_256));
            h->update(_path);
            return h->bytedigest().to<uint_64>(0);
        }

        template <typename MapT, typename CreateFn, typename DiscardFn, typename NotifyFn>
        static typename MapT::mapped_type pool_ref_impl(MapT& _map,
                                                        thread_safe_readwrite<void>& _lock,
                                                        uint_64 _key, CreateFn&& _create,
                                                        DiscardFn&& _discard,
                                                        NotifyFn&& _notify) {
            // find and ref are one step under the read lock: an erase cannot slip in between them
            {
                thread_safe_rlock rl(_lock);
                auto it = _map.find(_key);
                if (it != _map.end()) {
                    it->second->m_ref.ref();
                    return it->second;
                }
            }
            // created outside the locks: parse / dlopen can re-enter this pool from a host callback
            auto* created = _create();
            bool won = false;
            {
                thread_safe_wlock wl(_lock);
                auto it = _map.find(_key);
                if (it != _map.end()) {

                    if (created) _discard(created);
                    it->second->m_ref.ref();
                    return it->second;
                }
                if (!created) return nullptr;
                _map[_key] = created;
                won = true;
            }
            if (won) _notify(created);
            return created;
        }

        // true = this was the last reference, so the entry is gone and the caller may delete it
        template <typename MapT>
        static bool pool_release_impl(MapT& _map, thread_safe_readwrite<void>& _lock,
                                      typename MapT::mapped_type _r) {
            thread_safe_wlock wl(_lock);
            if (_r->m_ref.deref()) return false;
            auto it = _map.find(_r->m_id);
            if (it == _map.end()) return false;
            _map.erase(it);
            return true;
        }

        ast_resource* res_mng::ref_ast(const std::string& _path, walker* _w,
                                       script_exception& _err) {
            uint_64 key = res_make_key(_path);
            return pool_ref_impl(m_asts, m_lock, key, [&]() -> ast_resource* {
                                     std::unique_ptr<ast_resource> r(new ast_resource());
                                     r->m_id = key;
                                     r->m_path = _path;
                                     if (!load_module_ast(_path, _w, r.get())) {
                                         _err = {error_type::ImportError,
                                                 std::string("Cannot load module: ") + _path};
                                         return nullptr;
                                     }
                                     r->m_ref.init_owned();
                                     return r.release(); }, [](ast_resource* _r) { delete _r; }, [&](ast_resource*) { fire(&on_csys, "ast load: " + _path); });
        }

        link_resource* res_mng::ref_link(const std::string& _path, walker* _w,
                                         script_exception& _err) {
            (void) _w;
            uint_64 key = res_make_key(_path);
            // entry points stay unvalidated here: the instance layer null-tests each before calling
            return pool_ref_impl(m_links, m_lock, key, [&]() -> link_resource* {
#if defined(__linux__)
                                     void* h = dlopen(_path.c_str(), RTLD_NOW);
                                     if (!h) {
                                         const char* err = dlerror();
                                         fire(&on_csys,
                                              std::string("link error: dlopen failed: ") +
                                                  (err ? err : "unknown error"));
                                         _err = {error_type::ImportError,
                                                 std::string("Cannot link: ") + _path};
                                         return nullptr;
                                     }
                                     using bind_fn_t = void (*)(fwrap&);
                                     bind_fn_t load_fn = (bind_fn_t) dlsym(h, LINK_LOAD);
                                     script_create create_fn = (script_create) dlsym(h, LINK_CREATE);
                                     script_release release_fn = (script_release) dlsym(h, LINK_RELEASE);
                                     script_unload unload_fn = (script_unload) dlsym(h, LINK_UNLOAD);
#elif defined(_WIN32)
                                     HMODULE h = LoadLibraryA(_path.c_str());
                                     if (!h) {
                                         fire(&on_csys,
                                              std::string("link error: LoadLibrary failed: ") + _path);
                                         _err = {error_type::ImportError,
                                                 std::string("Cannot link: ") + _path};
                                         return nullptr;
                                     }
                                     using bind_fn_t = void (*)(fwrap&);
                                     bind_fn_t load_fn = (bind_fn_t) GetProcAddress(h, LINK_LOAD);
                                     script_create create_fn = (script_create) GetProcAddress(h, LINK_CREATE);
                                     script_release release_fn = (script_release) GetProcAddress(h, LINK_RELEASE);
                                     script_unload unload_fn = (script_unload) GetProcAddress(h, LINK_UNLOAD);
#endif

                                     std::unique_ptr<link_resource> r(new link_resource());
                                     r->m_id = key;
                                     r->m_path = _path;
                                     r->m_handle = h;
                                     r->m_load_fn = load_fn;
                                     r->m_create_fn = create_fn;
                                     r->m_release_fn = release_fn;
                                     r->m_unload_fn = unload_fn;
                                     r->m_ref.init_owned();
                                     return r.release(); }, [](link_resource* _r) {
                                     if (_r->m_handle) {
#if defined(__linux__)
                                         dlclose(_r->m_handle);
#elif defined(_WIN32)
                                         FreeLibrary((HMODULE) _r->m_handle);
#endif
                                     }
                                     delete _r; }, [&](link_resource*) { fire(&on_csys, "link load: " + _path); });
        }

        void res_mng::release_ast(ast_resource* _r) {
            if (!_r) return;
            if (!pool_release_impl(m_asts, m_lock, _r)) return;
            // erased from the pool but not deleted yet: the event still reads _r->m_path
            fire(&on_csys, "ast unload: " + _r->m_path);
            delete _r;
        }
        void res_mng::release_link(link_resource* _r) {
            if (!_r) return;
            if (!pool_release_impl(m_links, m_lock, _r)) return;
            fire(&on_csys, "link unload: " + _r->m_path);
            // only the library goes away here: m_unload_fn is the instance layer's to call
            if (_r->m_handle) {
#if defined(__linux__)
                dlclose(_r->m_handle);
#elif defined(_WIN32)
                FreeLibrary((HMODULE) _r->m_handle);
#endif
            }
            delete _r;
        }

        // refcounts are ignored: anything still held outside this pool dangles once this returns
        void res_mng::clear() {
            while (!m_asts.empty()) {
                ast_resource* r;
                {
                    thread_safe_wlock wl(m_lock);
                    auto it = m_asts.begin();
                    r = it->second;
                    m_asts.erase(it);
                }
                delete r;
            }
            while (!m_links.empty()) {
                link_resource* r;
                {
                    thread_safe_wlock wl(m_lock);
                    auto it = m_links.begin();
                    r = it->second;
                    m_links.erase(it);
                }
                if (r->m_handle) {
#if defined(__linux__)
                    dlclose(r->m_handle);
#elif defined(_WIN32)
                    FreeLibrary((HMODULE) r->m_handle);
#endif
                }
                delete r;
            }
        }

        bool res_mng::load_module_ast(const std::string& _path, walker* _w,
                                      ast_resource* _res) const {

            // fast path: the .axp-embedded module table -- an inner module need not exist as a file
            if (_w->state.compiled_modules) {
                if (_w->state.compiled_modules->contain(_path)) {
                    const variant& mod_v = _w->state.compiled_modules->value(_path);
                    if (mod_v.is_map()) {
                        const varmap& vm = mod_v.to<varmap>();
                        if (vm.contain("ast")) {
                            _res->m_ast = vm.value("ast").to<varvec>();
                            if (!_res->m_ast.empty()) return true;
                        }
                    }
                    if (mod_v.is_vec()) {
                        _res->m_ast = mod_v.to<varvec>();
                        if (!_res->m_ast.empty()) return true;
                    }
                }
            }

            std::string sfx = file_info(_path).suffix();
            // suffix dispatch: 1 = .axc, 2 = .axp, 0 = anything else, tried both ways
            int flag = (sfx == ".axc") ? 1 : (sfx == ".axp") ? 2
                                                             : 0;

            if (0 == flag || 2 == flag) {

                bytes content = file::read_all(_path);
                if (!content.empty() && varsolid::is_valid(bytes_view(content))) {
                    varmap outer = varsolid::to_varmap(bytes_view(content));
                    if (!outer.empty()) {
                        varmap inner_modules;
                        if (decompress_inner(outer, _res->m_ast, inner_modules)) {
                            _res->m_inner_modules = std::move(inner_modules);
                            return true;
                        }
                    }
                }

                if (!content.empty()) {
                    token_list tl;
                    tl.tokenize(bytes_view(content));
                    parser p(tl, nullptr, _path, *_w->state.m_search_paths,
                             _w->m_cfg.parse_depth, nullptr, nullptr, nullptr, std::string(), 0,
                             _w->m_cfg.debug_enable);
                    _res->m_ast = p.parse();
                    if (!p.has_error() && !_res->m_ast.empty()) return true;
                }
            }
            if (0 == flag || 1 == flag) {

                bytes content = file::read_all(_path);
                if (content.empty()) return false;
                token_list tl;
                tl.tokenize(bytes_view(content));
                parser p(tl, nullptr, _path, *_w->state.m_search_paths,
                         _w->m_cfg.parse_depth, nullptr, nullptr, nullptr, std::string(), 0,
                         _w->m_cfg.debug_enable);
                _res->m_ast = p.parse();
                return !p.has_error() && !_res->m_ast.empty();
            }
            return false;
        }

        // returns the resolved absolute path, not the caller's spelling: the pool is keyed on it
        std::string res_mng::resolve_runtime_path(const std::string& _path,
                                                  impl_import* _inst,
                                                  const std::list<std::string>& _search_paths,
                                                  const varmap* _compiled_modules) {
            if (_path.empty()) return {};
            if (_path[0] == '@' && _compiled_modules) {
                if (_compiled_modules->contain(_path)) return _path;
            }
            if (is_absolute_path(_path) && file_info(_path).is_exist())
                return file_info(_path).path();

            // instance env paths (own directory, replaced by env()) win over the configured ones
            if (_inst) {
                for (const auto& ep : _inst->m_env_paths) {
                    std::string joined = join_path(ep, _path);
                    if (file_info(joined).is_exist())
                        return file_info(joined).path();
                }
            }
            for (const auto& sp : _search_paths) {
                std::string joined = join_path(sp, _path);
                if (file_info(joined).is_exist())
                    return file_info(joined).path();
            }
            {
                file_info fi(_path);
                if (fi.is_exist()) return fi.path();
            }
            if (file_info(_path).is_exist())
                return file_info(_path).path();
            return {};
        }

        static bool has_extension(const std::string& _path) {
            auto pos = _path.find_last_of("/\\");
            auto name = (pos == std::string::npos) ? _path : _path.substr(pos + 1);
            return name.find('.') != std::string::npos;
        }

        std::string res_mng::resolve_runtime_path_auto_import(const std::string& _path,
                                                              impl_import* _inst,
                                                              const std::list<std::string>& _search_paths,
                                                              const varmap* _compiled_modules) {
            if (has_extension(_path)) return {};
            static const char* exts[] = {".axp", ".axc"};
            for (auto ext : exts) {
                std::string r = resolve_runtime_path(_path + ext, _inst, _search_paths, _compiled_modules);
                if (!r.empty()) return r;
            }
            return {};
        }

        std::string res_mng::resolve_runtime_path_auto_link(const std::string& _path,
                                                            impl_import* _inst,
                                                            const std::list<std::string>& _search_paths,
                                                            const varmap* _compiled_modules) {
            if (has_extension(_path)) return {};
#if defined(__linux__)
            static const char* exts[] = {".so"};
#elif defined(_WIN32)
            static const char* exts[] = {".dll"};
#elif defined(__APPLE__)
            static const char* exts[] = {".dylib"};
#else
            static const char* exts[] = {};
#endif
            for (auto ext : exts) {
                std::string r = resolve_runtime_path(_path + ext, _inst, _search_paths, _compiled_modules);
                if (!r.empty()) return r;
            }
            return {};
        }

    }
}
