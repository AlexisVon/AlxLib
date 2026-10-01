/*****************************************************************/ /**
 * \file   ascript_modmng.h
 * \brief  Per-engine module instance manager — fly pools (single-threaded, lock-free)
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#ifndef _ALEXIS_SCRIPT_MODMNG_H_
#define _ALEXIS_SCRIPT_MODMNG_H_

#include "ascript_base.h"
#include <unordered_map>
#include <vector>

namespace alx {
    namespace script {

        // per-engine pool of fly objects, single-threaded: one engine uses one mod_mng, one thread at a time
        // a fly holds its resource ref from birth to pool exit, so the last deref unloads the module
        class mod_mng {
        public:
            mod_mng();
            ~mod_mng();

            // points at the process-wide resource layer: the first mod_mng creates it, the last one deletes it
            res_mng* m_res = nullptr;

            // steps 1-3 below for one path; the result owns one fly ref, dropped by its destructor
            impl_import* make_import(const std::string& _import_file, walker* _w,
                                     script_exception& _err);
            impl_link* make_link(const std::string& _link_file, walker* _w,
                                 script_exception& _err);

            // 1. the hook fires first, then a pooled hit refs in place; a miss parses/dlopens and builds the fly
            fly_import* ref_fly_import(const std::string& _path, walker* _w,
                                       script_exception& _err);
            fly_link* ref_fly_link(const std::string& _path, walker* _w,
                                   script_exception& _err);

            // 2. one-shot gate: the body runs once per pool entry and its outcome (done or the error) is cached
            bool init_fly_import(fly_import* _fly, walker* _w, script_exception& _err);
            bool init_fly_link(fly_link* _fly, walker* _w, script_exception& _err);

            // 3. per-instance: copies the fly's template store (link: create_fn when the module has one)
            impl_import* impl_fly_import(fly_import* _fly, walker* _w,
                                         script_exception& _err);
            impl_link* impl_fly_link(fly_link* _fly, walker* _w,
                                     script_exception& _err);

            // drops one reference; at zero the entry is erased and the resource ref given back (link: unload_fn on the live template first)
            void release_fly(fly_import* _fw);
            void release_fly(fly_link* _fw);
            // teardown only, instances must already be gone: every entry is dropped, refs or not
            void clear();

        private:
            // s_users counts the live mod_mng objects; only construction and destruction take this lock
            static res_mng* s_res;
            static int s_users;
            static std::mutex s_res_mtx;

            // the shared gate body is defined in the .cpp; a friend only to reach m_loading_stack
            template <typename FlyT, typename DoInit>
            friend bool init_fly_gate(FlyT*, mod_mng*, walker*, script_exception&,
                                      const char*, const char*, DoInit&&);

            // keyed by res_make_key of the resolved path: one fly per resolved file, deleted when its refs hit zero
            std::unordered_map<uint_64, fly_import*> m_imports;
            std::unordered_map<uint_64, fly_link*> m_links;
            // resolved paths of the init chain in flight: a repeat means self- or circular import
            std::vector<std::string> m_loading_stack;
            // runs the module body in a sub-walker and keeps the store it leaves as the fly's template
            bool run_init_walk(fly_import* _fly, walker* _w, script_exception& _err);
        };

    }
}

#endif
