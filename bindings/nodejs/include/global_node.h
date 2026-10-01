/*****************************************************************/ /**
 * \file   global_node.h
 * \brief  Node.js addon — V8/Node type aliases and utilities
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#ifndef _ALEXIS_NODE_GLOBAL_H_
#define _ALEXIS_NODE_GLOBAL_H_

#include "node.h"
#include "node_object_wrap.h"

using v8::Global;
using v8::Local;
using v8::MaybeLocal;
using v8::Null;

using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;

using v8::Array;
using v8::ArrayBuffer;
using v8::Boolean;
using v8::Function;
using v8::Number;
using v8::Object;
using v8::ObjectTemplate;
using v8::ReturnValue;
using v8::String;
using v8::Symbol;
using v8::Template;
using v8::Uint8Array;
using v8::Value;

using v8::Context;
using v8::Exception;
using v8::HandleScope;
using v8::Isolate;
using v8::Locker;
using v8::Persistent;
using v8::TryCatch;

using node::AddEnvironmentCleanupHook;
using node::ObjectWrap;

class args_wrap;
typedef const FunctionCallbackInfo<Value>& NODE_ARGS;

#define REG_FUNCTION(NAME, EXPORTS)         \
    auto NAME##_interface =                 \
        [](NODE_ARGS args) { NAME(args); }; \
    NODE_SET_METHOD(EXPORTS, #NAME, NAME##_interface)

class node_base
    : public ObjectWrap {
    friend class args_wrap;
    template <typename T, node_base* (*) (NODE_ARGS)> friend class class_register;
};

template <typename T>
class node_object
    : public node_base {
public:

    template <typename... Args>
    static Local<Object> NewInstance(Isolate* _isolate, Args... _args) {
        Local<Object> ins = Local<Function>::New(_isolate, constructor)->NewInstance(_isolate->GetCurrentContext()).ToLocalChecked();
        ObjectWrap::Unwrap<T>(ins)->init(std::forward<Args>(_args)...);
        return ins;
    }

private:
    static Global<Function> constructor;
    friend class args_wrap;
    template <typename U, node_base* (*) (NODE_ARGS)> friend class class_register;
    static constexpr int fieldcount{1};
};
template <typename T>
Global<Function> node_object<T>::constructor;

template <typename T, node_base* (*NEW)(NODE_ARGS)>
class class_register {
public:
    class_register(const char* _name, Local<Object>& _exports)
        : m_name(_name), m_exports(_exports) {
        m_handle = FunctionTemplate::New(Isolate::GetCurrent(), &create_method);
        m_handle->InstanceTemplate()->SetInternalFieldCount(T::fieldcount);
    }
    ~class_register() {
        m_handle->SetClassName(String::NewFromUtf8(Isolate::GetCurrent(), m_name.c_str()).ToLocalChecked());
        T::constructor.Reset(Isolate::GetCurrent(),
                             m_handle->GetFunction(Isolate::GetCurrent()->GetCurrentContext()).ToLocalChecked());
        AddEnvironmentCleanupHook(Isolate::GetCurrent(), [](void* _p) { ((Global<Function>*) _p)->Reset(); }, &T::constructor);

        m_exports->Set(Isolate::GetCurrent()->GetCurrentContext(),
                       String::NewFromUtf8(Isolate::GetCurrent(), m_name.c_str()).ToLocalChecked(),
                       Local<Function>::New(Isolate::GetCurrent(), T::constructor))
            .Check();
    }
    template <void (T::*_func)(args_wrap)>
    void regist_method(const char* _name) {
        auto method = [](NODE_ARGS _args) { (ObjectWrap::Unwrap<T>(_args.This())->*_func)(_args); };
        NODE_SET_PROTOTYPE_METHOD(m_handle, _name, method);
    }
    static void create_method(NODE_ARGS _args) {
        if (_args.IsConstructCall()) {
            node_base* object = NEW(_args);
            if (object != nullptr) {
                object->Wrap(_args.This());
                _args.GetReturnValue().Set(_args.This());
            }
        } else {
            std::vector<Local<Value>> argv;
            for (int i = 0; i < _args.Length(); i++) argv.push_back(_args[i]);
            _args.GetReturnValue().Set(Local<Function>::New(_args.GetIsolate(), T::constructor)
                                           ->NewInstance(_args.GetIsolate()->GetCurrentContext(), (int) argv.size(), argv.empty() ? nullptr : &argv.front())
                                           .ToLocalChecked());
        }
    }

private:
    const std::string m_name;
    Local<Object>& m_exports;
    Local<FunctionTemplate> m_handle;
};

#endif