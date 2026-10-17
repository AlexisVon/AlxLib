/*****************************************************************/ /**
 * \file   ascript_resmng.h
 * \brief  Resource layer — shared singleton, read-only/stateless resources
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#ifndef _ALEXIS_SCRIPT_RESMNG_H_
#define _ALEXIS_SCRIPT_RESMNG_H_

#include "ascript_base.h"
#include "athread_safe.h"
#include <atomic>
#include <condition_variable>
#include <list>
#include <mutex>
#include <thread>

namespace alx {
    namespace script {

        // shared across every engine in the process, and immutable once pooled -- the only multi-threaded surface here
        struct ast_resource {
            uint_64 m_id;
            std::string m_path;
            // call_able::m_def points into this storage, so a fly's ref is what pins it
            varvec m_ast;
            // .axp embedded module table (empty for .axc) that resolves its nested imports
            varmap m_inner_modules;
            ref_count m_ref;
        };

        struct link_resource {
            uint_64 m_id;
            std::string m_path;
            void* m_handle = nullptr;
            // dlsym results only; the instance layer calls them, never this pool
            script_load m_load_fn = nullptr;
            script_create m_create_fn = nullptr;
            script_release m_release_fn = nullptr;
            script_unload m_unload_fn = nullptr;
            ref_count m_ref;
        };

        uint_64 res_make_key(const std::string& _path);

        // the periodic receiving face: the heartbeat thread calls time_event() on every registered
        // target once per pass; later periodic duties join this surface as more virtuals
        class hb_target {
        public:
            virtual ~hb_target() = default;
            virtual void time_event() = 0;
        };

        class res_mng {
        public:
            res_mng();
            ~res_mng();

            // an engine must outlive its registration: attach at create, detach before its walker dies
            void hb_attach(hb_target* _t);
            void hb_detach(hb_target* _t);

            // resource events only: "ast load/unload:", "link load/unload:", "link error: ..."
            alx::signal<uint_64, const std::string&> on_csys;

            // failure -> nullptr + _err; a success takes one ref that must be released exactly once
            ast_resource* ref_ast(const std::string& _path, walker* _w,
                                  script_exception& _err);
            link_resource* ref_link(const std::string& _path, walker* _w,
                                    script_exception& _err);

            // last ref -> evict (ast: delete, link: dlclose); the next ref re-reads the file
            void release_ast(ast_resource* _r);
            void release_link(link_resource* _r);

            void clear();

            static std::string resolve_runtime_path(const std::string& _path,
                                                    impl_import* _inst,
                                                    const std::list<std::string>& _search_paths,
                                                    const varmap* _compiled_modules = nullptr);
            static std::string resolve_runtime_path_auto_import(const std::string& _path,
                                                                impl_import* _inst,
                                                                const std::list<std::string>& _search_paths,
                                                                const varmap* _compiled_modules = nullptr);
            static std::string resolve_runtime_path_auto_link(const std::string& _path,
                                                              impl_import* _inst,
                                                              const std::list<std::string>& _search_paths,
                                                              const varmap* _compiled_modules = nullptr);

        private:
            thread_safe_readwrite<void> m_lock;
            std::unordered_map<uint_64, ast_resource*> m_asts;
            std::unordered_map<uint_64, link_resource*> m_links;
            bool load_module_ast(const std::string& _path, walker* _w,
                                 ast_resource* _res) const;

            void hb_loop();
            // the registry lock spans a whole pass, so a detach cannot race a dying target
            std::mutex m_hb_mtx;
            std::list<hb_target*> m_hb;
            // the period wait, separate so attach/detach never block behind a sleeping pass
            std::mutex m_hb_wait_mtx;
            std::condition_variable m_hb_cv;
            std::thread m_hb_thread;
            std::atomic<bool> m_hb_stop{false};
        };

    }
}

#endif
