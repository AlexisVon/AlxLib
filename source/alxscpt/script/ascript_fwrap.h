/*****************************************************************/ /**
 * \file   ascript_fwrap.h
 * \brief  fwrap implementations — one class per call context
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

        // Store face shared by every context; the call face and the registrations a context does
        // not allow refuse with LinkError here, and each concrete class overrides what it allows
        class fwrap_common_impl : public fwrap {
        public:
            fwrap_common_impl(data_store* _store, walker* _ws) : m_store(_store), m_ws(_ws) {}

            size_t size() const override;
            variant& operator[](size_t _i) const override;
            void freturn(const variant& _v) override;
            void freturn(variant&& _v) override;
            variant* iload(const std::string& _key) override;
            variant call(const variant& _func, const varvec& _args) override;
            void raise(const variant& _info, error_type _what = error_type::RuntimeError) override;
            bool bind(const std::string& _name, void (*_func)(fwrap&),
                      const std::string& _area = std::string()) override;
            bool bind(const std::string& _area, anyptr&& _obj,
                      const std::map<std::string, void (*)(fwrap&)>& _methods) override;
            bool bind(uint_64 _rid, const char* _name,
                      std::string (*_desc)(const anyptr&) = nullptr,
                      variant (*_view)(const anyptr&) = nullptr) override;
            anyptr* object() override;
            variant* load(const std::string& _name) override;
            variant* store(const std::string& _name, const variant& _val) override;
            variant* store(const std::string& _name, variant&& _val) override;
            bool remove(const std::string& _name) override;
            const engine_config& config() const override;
            fwrap::callback* currying(const std::string& _name) override;

        protected:
            // the bind(native) body the load and create entries share
            bool bind_native(const std::string& _name, void (*_func)(fwrap&), const std::string& _area);
            // the bind(area, object, methods) body
            bool bind_area(const std::string& _area, anyptr&& _obj,
                           const std::map<std::string, void (*)(fwrap&)>& _methods);

            data_store* m_store;
            walker* m_ws;
            static const engine_config s_empty_cfg;
        };

        // A native call: the full interface
        class fwrap_native : public fwrap_common_impl {
        public:
            // _ret is null when the call site drops the value
            fwrap_native(std::vector<variant*>&& _args, variant* _ret,
                         data_store* _store, walker* _ws);

            size_t size() const override;
            variant& operator[](size_t _i) const override;
            void freturn(const variant& _v) override;
            void freturn(variant&& _v) override;
            variant* iload(const std::string& _key) override;
            variant call(const variant& _func, const varvec& _args) override;
            anyptr* object() override;
            fwrap::callback* currying(const std::string& _name) override;
            bool bind(const std::string& _name, void (*_func)(fwrap&),
                      const std::string& _area = std::string()) override;
            bool bind(const std::string& _area, anyptr&& _obj,
                      const std::map<std::string, void (*)(fwrap&)>& _methods) override;

            // Set by the engine when the native is reached through an area; stays null for a plain $name() call
            link_area* m_area = nullptr;

        private:
            std::vector<variant*> m_args;
            variant* m_ret;
        };

        // LINK_LOAD and LINK_CREATE: the store face plus native binding
        class fwrap_entry_impl : public fwrap_common_impl {
        public:
            explicit fwrap_entry_impl(data_store* _store) : fwrap_common_impl(_store, nullptr) {}
            bool bind(const std::string& _name, void (*_func)(fwrap&),
                      const std::string& _area = std::string()) override;
            bool bind(const std::string& _area, anyptr&& _obj,
                      const std::map<std::string, void (*)(fwrap&)>& _methods) override;
        };

        // LINK_LOAD, the only context that takes type registrations
        class fwrap_load : public fwrap_entry_impl {
        public:
            explicit fwrap_load(data_store* _store) : fwrap_entry_impl(_store) {}
            bool bind(uint_64 _rid, const char* _name,
                      std::string (*_desc)(const anyptr&) = nullptr,
                      variant (*_view)(const anyptr&) = nullptr) override;

            // Set by the engine: the module layer the registrations land in
            mod_mng* m_mgr = nullptr;
            // Registrations collected by the load entry; committed only when it returns normally
            std::vector<type_reg> m_pending;
        };

        // LINK_CREATE: like load, without type registration
        class fwrap_create : public fwrap_entry_impl {
        public:
            explicit fwrap_create(data_store* _store) : fwrap_entry_impl(_store) {}
        };

        // LINK_RELEASE and LINK_UNLOAD: the store face only
        class fwrap_teardown : public fwrap_common_impl {
        public:
            explicit fwrap_teardown(data_store* _store) : fwrap_common_impl(_store, nullptr) {}
        };

    }
}

#endif
