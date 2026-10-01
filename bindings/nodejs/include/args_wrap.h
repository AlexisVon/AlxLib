/*****************************************************************/ /**
 * \file   args_wrap.h
 * \brief  Node.js addon — args_wrap declaration
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#ifndef _ALEXIS_NODE_ARGS_WRAP_H_
#define _ALEXIS_NODE_ARGS_WRAP_H_

#include <abytes.h>
#include <autility.h>
#include <avariant.h>
#include <vector>

#include "global_node.h"
#include "v8-array-buffer.h"

using namespace alx;

class args_wrap {
public:
    args_wrap(NODE_ARGS _args);

public:
    inline Local<Value> operator[](int _index) {
        if (_index < 0 || _index >= m_args.Length()) return Local<Value>();
        return m_args[_index];
    }
    inline int size() { return m_args.Length(); }

    template <typename T>
    inline T* Unwrap() { return ObjectWrap::Unwrap<T>(m_args.This()); }

    inline Isolate* getIsolate() { return m_isolate; }
    inline Local<Context> getContext() { return m_context; }

    inline void SetReturn(const bytes_view& _data) { m_args.GetReturnValue().Set(toLArrayBuffer(_data, m_isolate)); }
    inline void SetReturn(const std::string& _data) { m_args.GetReturnValue().Set(toLString(_data, m_isolate)); }
    inline void SetReturn(Local<Value> _value) { m_args.GetReturnValue().Set(_value); }
    inline void SetReturn(node_base* _object) {
        _object->Wrap(m_args.This());
        m_args.GetReturnValue().Set(m_args.This());
    }

    operator const FunctionCallbackInfo<Value>&() { return m_args; }

public:
    enum exception_type : int {
        RangeError = 0,
        ReferenceError,
        SyntaxError,
        TypeError,
        WasmCompileError,
        WasmLinkError,
        WasmRuntimeError,
        Error = 0X7FFFFFFF,
    };
    void exception(const std::string& _message, exception_type _type = Error);

public:
    bool isInt32(int _index);
    bool isUint32(int _index);
    bool isNumber(int _index);
    bool isString(int _index);
    bool isBool(int _index);
    bool isCallback(int _index);
    bool isArrayBuffer(int _index);
    bool isObject(int _index);

    int_32 getInt32(int _index, const int_32 _default = 0.0);
    uint_32 getUint32(int _index, const uint_32 _default = 0.0);
    double getNumber(int _index, const double _default = 0.0);
    std::string getString(int _index, const std::string& _default = std::string());
    bytes getArrayBuffer(int _index, const bytes& _default = bytes());
    bool getBool(int _index, const bool& _default = false);
    Local<Object> getObject(int _index);
    MaybeLocal<Function> getCallback(int _index);

public:
    inline static std::string toString(const Local<String>& _value, Isolate* _isolate) {
        String::Utf8Value _str(_isolate, _value);
        return std::string(*_str, _str.length());
    }
    template <typename T, typename TArray>
    inline static std::vector<T> toVector(const Local<TArray>& _value, Isolate* _isolate) {
        size_t _size = _value->Length();
        std::vector<T> _result(_size);
        Local<v8::ArrayBufferView> view = Local<v8::ArrayBufferView>::Cast(_value);
        T* ptr = reinterpret_cast<T*>(static_cast<char*>(view->Buffer()->GetBackingStore()->Data()) + view->ByteOffset());
        memcpy(_result.data(), ptr, _size * sizeof(T));
        return _result;
    }
    static variant toVariant(const Local<Value>& _value, Isolate* _isolate);
    static varvec toVarvec(const Local<Array>& _value, Isolate* _isolate);
    static varmap toVarmap(const Local<Object>& _value, Isolate* _isolate);

public:
    template <typename TArray, typename T>
    static Local<TArray> toLTypeArray(const std::vector<T>& _value, Isolate* _isolate) {
        return TArray::New(toLArrayBuffer(const_cast<T*>(_value.data()), _value.size() * sizeof(T), _isolate, true), 0, _value.size());
    }
    static Local<Value> toLValue(const variant& _value, Isolate* _isolate);
    static Local<Array> toLArray(const varvec& _value, Isolate* _isolate);
    static Local<Array> toLArray(const varlst& _value, Isolate* _isolate);
    static Local<Object> toLObject(const varmap& _value, Isolate* _isolate);

    static Local<String> toLString(const std::string& _str, Isolate* _isolate);
    static Local<ArrayBuffer> toLArrayBuffer(const bytes_view& _data, Isolate* _isolate);
    static Local<ArrayBuffer> toLArrayBuffer(void* _data, size_t _size, Isolate* _isolate, bool _copy = true);

private:
    Isolate* m_isolate{nullptr};
    Local<Context> m_context;
    NODE_ARGS m_args;
};

#endif