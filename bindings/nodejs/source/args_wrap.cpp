/*****************************************************************/ /**
 * \file   args_wrap.cpp
 * \brief  Node.js addon — V8 argument conversion helpers
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "args_wrap.h"
#include "abase.h"
#include "v8-array-buffer.h"
#include "v8-container.h"
#include "v8-context.h"
#include "v8-local-handle.h"
#include "v8-object.h"
#include "v8-primitive.h"
#include "v8-typed-array.h"
#include "v8-value.h"

args_wrap::args_wrap(NODE_ARGS _args)
    : m_args(_args) {
    m_isolate = _args.GetIsolate();
    m_context = m_isolate->GetCurrentContext();
}

void args_wrap::exception(const std::string& _message, exception_type _type) {
    Local<String> message = toLString(_message, m_isolate);
    switch (_type) {
    case args_wrap::RangeError:
        m_isolate->ThrowException(Exception::RangeError(message));
        break;
    case args_wrap::ReferenceError:
        m_isolate->ThrowException(Exception::ReferenceError(message));
        break;
    case args_wrap::SyntaxError:
        m_isolate->ThrowException(Exception::SyntaxError(message));
        break;
    case args_wrap::TypeError:
        m_isolate->ThrowException(Exception::TypeError(message));
        break;
    case args_wrap::WasmCompileError:
        m_isolate->ThrowException(Exception::WasmCompileError(message));
        break;
    case args_wrap::WasmLinkError:
        m_isolate->ThrowException(Exception::WasmLinkError(message));
        break;
    case args_wrap::WasmRuntimeError:
        m_isolate->ThrowException(Exception::WasmRuntimeError(message));
        break;
    case args_wrap::Error:
    default:
        m_isolate->ThrowError(message);
        break;
    }
}

bool args_wrap::isInt32(int _index) { return _index >= 0 && _index < m_args.Length() && m_args[_index]->IsInt32(); }
bool args_wrap::isUint32(int _index) { return _index >= 0 && _index < m_args.Length() && m_args[_index]->IsUint32(); }
bool args_wrap::isNumber(int _index) { return _index >= 0 && _index < m_args.Length() && m_args[_index]->IsNumber(); }
bool args_wrap::isString(int _index) { return _index >= 0 && _index < m_args.Length() && m_args[_index]->IsString(); }
bool args_wrap::isBool(int _index) { return _index >= 0 && _index < m_args.Length() && m_args[_index]->IsBoolean(); }
bool args_wrap::isCallback(int _index) { return _index >= 0 && _index < m_args.Length() && m_args[_index]->IsFunction(); }
bool args_wrap::isArrayBuffer(int _index) { return _index >= 0 && _index < m_args.Length() && m_args[_index]->IsArrayBuffer(); }
bool args_wrap::isObject(int _index) { return _index >= 0 && _index < m_args.Length() && m_args[_index]->IsObject(); }

int_32 args_wrap::getInt32(int _index, const int_32 _default) {
    if (!isInt32(_index)) return _default;
    return m_args[_index].As<v8::Int32>()->Value();
}

uint_32 args_wrap::getUint32(int _index, const uint_32 _default) {
    if (!isUint32(_index)) return _default;
    return m_args[_index].As<v8::Uint32>()->Value();
}

double args_wrap::getNumber(int _index, const double _default) {
    if (!isNumber(_index)) return _default;
    return m_args[_index].As<Number>()->Value();
}

std::string args_wrap::getString(int _index, const std::string& _default) {
    if (!isString(_index)) return _default;
    bytes buffer;
    Local<String> str = m_args[_index].As<String>();
    buffer.resize(str->Utf8LengthV2(m_isolate));
    str->WriteUtf8V2(m_isolate, (char*) buffer.data(), (int) buffer.size());
    return std::string((char*) buffer.data(), (int) buffer.size());
}

bytes args_wrap::getArrayBuffer(int _index, const bytes& _default) {
    if (!isArrayBuffer(_index)) return _default;
    Local<ArrayBuffer> buffer = m_args[_index].As<ArrayBuffer>();
    return bytes(buffer->Data(), buffer->ByteLength());
}

bool args_wrap::getBool(int _index, const bool& _default) {
    if (!isBool(_index)) return _default;
    return m_args[_index].As<Boolean>()->BooleanValue(m_isolate);
}

Local<Object> args_wrap::getObject(int _index) {
    if (!isObject(_index)) return Local<Object>();
    return m_args[_index].As<Object>();
}

MaybeLocal<Function> args_wrap::getCallback(int _index) {
    if (!isCallback(_index)) return MaybeLocal<Function>();
    return m_args[_index].As<Function>();
}

variant args_wrap::toVariant(const Local<Value>& _value, Isolate* _isolate) {
    if (_value->IsBoolean()) return _value.As<Boolean>()->Value();
    if (_value->IsInt32()) return _value.As<v8::Int32>()->Value();
    if (_value->IsUint32()) return _value.As<v8::Uint32>()->Value();
    if (_value->IsBigInt()) {
        Local<v8::BigInt> bigint = _value.As<v8::BigInt>();
        bool lossless = false;
        int_64 i64 = bigint->Int64Value(&lossless);
        if (lossless) return i64;
        uint_64 ui64 = bigint->Uint64Value(&lossless);
        if (lossless) return ui64;
        return toString(bigint->ToString(_isolate->GetCurrentContext()).ToLocalChecked(), _isolate);
    }
    if (_value->IsNumber()) return _value.As<Number>()->Value();
    if (_value->IsString()) return toString(_value.As<String>(), _isolate);

    if (_value->IsObject()) {
        if (_value->IsArray()) return toVarvec(_value.As<Array>(), _isolate);
        else if (_value->IsTypedArray()) {
            if (_value->IsInt8Array()) return toVector<int_8>(_value.As<v8::Int8Array>(), _isolate);
            else if (_value->IsUint8Array()) return toVector<uint_8>(_value.As<v8::Uint8Array>(), _isolate);
            else if (_value->IsUint8ClampedArray()) return toVector<uint_8>(_value.As<v8::Uint8ClampedArray>(), _isolate);
            else if (_value->IsInt16Array()) return toVector<int_16>(_value.As<v8::Int16Array>(), _isolate);
            else if (_value->IsUint16Array()) return toVector<uint_16>(_value.As<v8::Uint16Array>(), _isolate);
            else if (_value->IsInt32Array()) return toVector<int_32>(_value.As<v8::Int32Array>(), _isolate);
            else if (_value->IsUint32Array()) return toVector<uint_32>(_value.As<v8::Uint32Array>(), _isolate);
            else if (_value->IsFloat32Array()) return toVector<real_32>(_value.As<v8::Float32Array>(), _isolate);
            else if (_value->IsFloat64Array()) return toVector<real_64>(_value.As<v8::Float64Array>(), _isolate);
            else if (_value->IsBigInt64Array()) return toVector<int_64>(_value.As<v8::BigInt64Array>(), _isolate);
            else if (_value->IsBigUint64Array()) return toVector<uint_64>(_value.As<v8::BigUint64Array>(), _isolate);
            else return variant();
        } else if (_value->IsArrayBuffer()) {
            Local<ArrayBuffer> buffer = _value.As<ArrayBuffer>();
            auto backing_store = buffer->GetBackingStore();
            return backing_store && backing_store->Data() ? bytes(backing_store->Data(), backing_store->ByteLength()) : bytes();
        } else return toVarmap(_value.As<Object>(), _isolate);
    }
    return variant();
}

varvec args_wrap::toVarvec(const Local<Array>& _value, Isolate* _isolate) {
    Local<Context> _context = _isolate->GetCurrentContext();
    varvec result;
    const uint32_t len = _value->Length();
    for (uint32_t i = 0; i < len; i++)
        result.push_back(toVariant(_value->Get(_context, i).ToLocalChecked(), _isolate));
    return result;
}

varmap args_wrap::toVarmap(const Local<Object>& _value, Isolate* _isolate) {
    Local<Context> _context = _isolate->GetCurrentContext();
    Local<Array> keys = _value->GetPropertyNames(_context).ToLocalChecked();
    varmap result;
    const uint32_t len = keys->Length();
    for (uint32_t i = 0; i < len; i++) {
        Local<Value> key = keys->Get(_context, i).ToLocalChecked();
        if (!key->IsString()) continue;
        String::Utf8Value key_utf8(_isolate, key);
        result[std::string(*key_utf8, key_utf8.length())] =
            toVariant(_value->Get(_context, key).ToLocalChecked(), _isolate);
    }
    return result;
}

Local<Value> args_wrap::toLValue(const variant& _value, Isolate* _isolate) {
    switch (_value.type()) {
    case variant::id<bool>(): return v8::Boolean::New(_isolate, _value.to<bool>());
    case variant::id<int_8>(): return v8::Int32::New(_isolate, _value.to<int_8>());
    case variant::id<int_16>(): return v8::Int32::New(_isolate, _value.to<int_16>());
    case variant::id<int_32>(): return v8::Int32::New(_isolate, _value.to<int_32>());
    case variant::id<int_64>(): return v8::BigInt::New(_isolate, _value.to<int_64>());
    case variant::id<uint_8>(): return v8::Uint32::New(_isolate, _value.to<uint_8>());
    case variant::id<uint_16>(): return v8::Uint32::New(_isolate, _value.to<uint_16>());
    case variant::id<uint_32>(): return v8::Uint32::New(_isolate, _value.to<uint_32>());
    case variant::id<uint_64>(): return v8::BigInt::NewFromUnsigned(_isolate, _value.to<uint_64>());
    case variant::id<real_32>(): return v8::Number::New(_isolate, _value.to<real_32>());
    case variant::id<real_64>(): return v8::Number::New(_isolate, _value.to<real_64>());
    case variant::id<std::string>(): return toLString(_value.to<std::string>(), _isolate);
    case variant::id<std::vector<int_8>>(): return toLTypeArray<v8::Int8Array>(_value.to<std::vector<int_8>>(), _isolate);
    case variant::id<std::vector<int_16>>(): return toLTypeArray<v8::Int16Array>(_value.to<std::vector<int_16>>(), _isolate);
    case variant::id<std::vector<int_32>>(): return toLTypeArray<v8::Int32Array>(_value.to<std::vector<int_32>>(), _isolate);
    case variant::id<std::vector<int_64>>(): return toLTypeArray<v8::BigInt64Array>(_value.to<std::vector<int_64>>(), _isolate);
    case variant::id<std::vector<uint_8>>(): return toLTypeArray<v8::Uint8Array>(_value.to<std::vector<uint_8>>(), _isolate);
    case variant::id<std::vector<uint_16>>(): return toLTypeArray<v8::Uint16Array>(_value.to<std::vector<uint_16>>(), _isolate);
    case variant::id<std::vector<uint_32>>(): return toLTypeArray<v8::Uint32Array>(_value.to<std::vector<uint_32>>(), _isolate);
    case variant::id<std::vector<uint_64>>(): return toLTypeArray<v8::BigUint64Array>(_value.to<std::vector<uint_64>>(), _isolate);
    case variant::id<std::vector<real_32>>(): return toLTypeArray<v8::Float32Array>(_value.to<std::vector<real_32>>(), _isolate);
    case variant::id<std::vector<real_64>>(): return toLTypeArray<v8::Float64Array>(_value.to<std::vector<real_64>>(), _isolate);
    case variant::id<bytes>(): return toLArrayBuffer(_value.to<bytes>(), _isolate);
    case variant::id<varlst>(): return toLArray(_value.to<varlst>(), _isolate);
    case variant::id<varvec>(): return toLArray(_value.to<varvec>(), _isolate);
    case variant::id<varmap>(): return toLObject(_value.to<varmap>(), _isolate);
    default: return Local<Value>();
    }
}

Local<Array> args_wrap::toLArray(const varvec& _value, Isolate* _isolate) {
    Local<Array> result = Array::New(_isolate, _value.size());
    Local<Context> _context = _isolate->GetCurrentContext();
    uint32_t idx{0};
    for (const auto& it : _value)
        (void) result->Set(_context, idx++, toLValue(it, _isolate));
    return result;
}

Local<Array> args_wrap::toLArray(const varlst& _value, Isolate* _isolate) {
    Local<Array> result = Array::New(_isolate, (int)_value.size());
    Local<Context> _context = _isolate->GetCurrentContext();
    uint32_t idx{0};
    for (const auto& it : _value)
        (void) result->Set(_context, idx++, toLValue(it, _isolate));
    return result;
}

Local<Object> args_wrap::toLObject(const varmap& _value, Isolate* _isolate) {
    Local<Object> result = Object::New(_isolate);
    Local<Context> _context = _isolate->GetCurrentContext();
    for (const auto& it : _value)
        (void) result->Set(_context, toLString(it.first, _isolate), toLValue(*it.second, _isolate));
    return result;
}

Local<String> args_wrap::toLString(const std::string& _str, Isolate* _isolate) {
    return String::NewFromUtf8(_isolate, _str.data()).ToLocalChecked();
}

Local<ArrayBuffer> args_wrap::toLArrayBuffer(const bytes_view& _data, Isolate* _isolate) {
    return toLArrayBuffer((void*) _data.data(), _data.size(), _isolate, true);
}

Local<ArrayBuffer> args_wrap::toLArrayBuffer(void* _data, size_t _size, Isolate* _isolate, bool _copy) {
    if (_copy) {
        void* data = new uint8_t[_size];
        memcpy(data, _data, _size);
        auto deleter = [](void* buf, size_t, void*) { delete[] static_cast<uint8_t*>(buf); };
        auto backing_store = ArrayBuffer::NewBackingStore(data, _size, deleter, nullptr);
        return ArrayBuffer::New(_isolate, std::move(backing_store));
    } else {
        auto deleter = [](void*, size_t, void*) {};
        auto backing_store = ArrayBuffer::NewBackingStore(_data, _size, deleter, nullptr);
        return ArrayBuffer::New(_isolate, std::move(backing_store));
    }
}
