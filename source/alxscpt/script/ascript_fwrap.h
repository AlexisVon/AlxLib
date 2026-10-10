/*****************************************************************/ /**
 * \file   ascript_fwrap.h
 * \brief  fwrap implementation — per-call native interface concrete class
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#ifndef _ALEXIS_SCRIPT_FWRAP_H_
#define _ALEXIS_SCRIPT_FWRAP_H_

#include "ascript.h"
#include "ascript_utils.h"
#include "ascript_walk.h"

namespace alx {
    namespace script {

        class fwrap_impl : public fwrap {
        public:
            // _ret and _ws are null in the module loader's entry points (load/create/unload/release)
            fwrap_impl(std::vector<variant*>&& _args, variant* _ret,
                       data_store* _store, walker* _ws);

            size_t size() const override;
            variant& operator[](size_t _i) const override;
            void freturn(const variant& _v) override;
            void freturn(variant&& _v) override;

            variant* iload(const std::string& _key) override;

            variant call(const variant& _func, const varvec& _args) override;
            void raise(const variant& _info,
                       error_type _what = error_type::RuntimeError) override;
            void bind(const std::string& _name,
                      void (*_func)(fwrap&),
                      const std::string& _area = std::string()) override;
            anyptr* object() override;
            variant* load(const std::string& _name) override;
            variant* store(const std::string& _name, const variant& _val) override;
            variant* store(const std::string& _name, variant&& _val) override;
            bool remove(const std::string& _name) override;

            const engine_config& config() const override { return m_ws ? m_ws->m_cfg : s_empty_cfg; }

            // Set by the engine when the native is reached through an area; stays null for a plain $name() call
            link_area* m_area = nullptr;

        private:
            // Non-owning pointers into script slots and call-site temps, live for this call only
            std::vector<variant*> m_args;
            variant* m_ret;
            data_store* m_store;
            walker* m_ws;
            static const engine_config s_empty_cfg;
        };

    }
}

#endif
