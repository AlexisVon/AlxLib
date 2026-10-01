/*****************************************************************/ /**
 * \file   varsolid_node.cpp
 * \brief  V8-direct deserialization from binary (skips varmap)
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "varsolid_node.h"
#include "aserial.h"
#include "v8-array-buffer.h"
#include "v8-container.h"
#include "v8-context.h"
#include "v8-local-handle.h"
#include "v8-object.h"
#include "v8-primitive.h"
#include "v8-typed-array.h"
#include "v8-value.h"
#include <algorithm>
#include <list>
#include <string>

using namespace alx;
using namespace alx::ser;

namespace alx {
    namespace varsolid_node {
        class deserer_v8 {
        public:
            typedef deserer<varmap, variant>::METADAT METADAT;
            typedef void (*COREFUNC)(const METADAT&, Local<Value>&, Isolate*);

            static std::unordered_map<BIT16, COREFUNC> CORE_BASE;
            static std::unordered_map<BIT16, COREFUNC> CORE__VEC;
            static std::unordered_map<BIT16, COREFUNC> CORE_LIST;

        private:

            template <typename TArray, typename TElem>
            static Local<TArray> make_typed_array(const BIT8* _data, BIT32 _size, Isolate* _isolate) {
                void* buf = new uint8_t[_size];
                memcpy(buf, _data, _size);
                auto deleter = [](void* ptr, size_t, void*) { delete[] static_cast<uint8_t*>(ptr); };
                auto backing = ArrayBuffer::NewBackingStore(buf, _size, deleter, nullptr);
                auto ab = ArrayBuffer::New(_isolate, std::move(backing));
                return TArray::New(ab, 0, _size / sizeof(TElem));
            }

            template <typename T>
            static void base_int_like(const METADAT& _dat, Local<Value>& _rst, Isolate* _isolate) {
                _rst = v8::Int32::New(_isolate, (int32_t) r_interpret<T>(_dat.ptr));
            }
            template <typename T>
            static void base_uint_like(const METADAT& _dat, Local<Value>& _rst, Isolate* _isolate) {
                _rst = v8::Uint32::New(_isolate, (uint32_t) r_interpret<T>(_dat.ptr));
            }

            static void base_bool(const METADAT& _dat, Local<Value>& _rst, Isolate* _isolate) {
                _rst = v8::Boolean::New(_isolate, r_interpret<bool>(_dat.ptr));
            }
            static void base_int64(const METADAT& _dat, Local<Value>& _rst, Isolate* _isolate) {
                _rst = v8::BigInt::New(_isolate, r_interpret<int_64>(_dat.ptr));
            }
            static void base_uint64(const METADAT& _dat, Local<Value>& _rst, Isolate* _isolate) {
                _rst = v8::BigInt::NewFromUnsigned(_isolate, r_interpret<uint_64>(_dat.ptr));
            }
            static void base_real32(const METADAT& _dat, Local<Value>& _rst, Isolate* _isolate) {
                _rst = v8::Number::New(_isolate, r_interpret<real_32>(_dat.ptr));
            }
            static void base_real64(const METADAT& _dat, Local<Value>& _rst, Isolate* _isolate) {
                _rst = v8::Number::New(_isolate, r_interpret<real_64>(_dat.ptr));
            }
            static void base_bytes(const METADAT& _dat, Local<Value>& _rst, Isolate* _isolate) {
                void* buf = new uint8_t[_dat.size];
                memcpy(buf, _dat.ptr, _dat.size);
                auto deleter = [](void* ptr, size_t, void*) { delete[] static_cast<uint8_t*>(ptr); };
                auto backing = ArrayBuffer::NewBackingStore(buf, _dat.size, deleter, nullptr);
                _rst = ArrayBuffer::New(_isolate, std::move(backing));
            }
            static void base_string(const METADAT& _dat, Local<Value>& _rst, Isolate* _isolate) {
                _rst = String::NewFromUtf8(_isolate, (char*) _dat.ptr, v8::NewStringType::kNormal, (int) _dat.size).ToLocalChecked();
            }

            template <typename TArray, typename TElem>
            static void vec_typed(const METADAT& _dat, Local<Value>& _rst, Isolate* _isolate) {
                if (0 == _dat.size) {
                    _rst = TArray::New(ArrayBuffer::New(_isolate, 0), 0, 0);
                    return;
                }
                _rst = make_typed_array<TArray, TElem>(_dat.ptr, _dat.size, _isolate);
            }
            static void vec_bool(const METADAT& _dat, Local<Value>& _rst, Isolate* _isolate) {
                if (0 == _dat.size) {
                    _rst = v8::Uint8Array::New(ArrayBuffer::New(_isolate, 0), 0, 0);
                    return;
                }
                _rst = make_typed_array<v8::Uint8Array, uint_8>(_dat.ptr, _dat.size, _isolate);
            }
            static void vec_bytes(const METADAT& _dat, Local<Value>& _rst, Isolate* _isolate) {
                Local<Array> arr = Array::New(_isolate);
                Local<Context> context = _isolate->GetCurrentContext();
                uint32_t idx = 0;
                deserer<varmap, variant>::for_each_meta(_dat.ptr, _dat.ptr + _dat.size, [&](const METADAT& meta) {
                    Local<Value> elem;
                    if (_bytes_ == (TYPE) meta.type) base_bytes(meta, elem, _isolate);
                    (void) arr->Set(context, idx++, elem);
                    return true;
                });
                _rst = arr;
            }
            static void vec_string(const METADAT& _dat, Local<Value>& _rst, Isolate* _isolate) {
                Local<Array> arr = Array::New(_isolate);
                Local<Context> context = _isolate->GetCurrentContext();
                uint32_t idx = 0;
                deserer<varmap, variant>::for_each_meta(_dat.ptr, _dat.ptr + _dat.size, [&](const METADAT& meta) {
                    Local<Value> elem;
                    base_string(meta, elem, _isolate);
                    (void) arr->Set(context, idx++, elem);
                    return true;
                });
                _rst = arr;
            }
            static void vec_any(const METADAT& _dat, Local<Value>& _rst, Isolate* _isolate) {
                Local<Array> arr = Array::New(_isolate);
                Local<Context> context = _isolate->GetCurrentContext();
                uint32_t idx = 0;
                deserer<varmap, variant>::for_each_meta(_dat.ptr, _dat.ptr + _dat.size, [&](const METADAT& meta) {
                    Local<Value> elem;
                    read_dat(meta, elem, _isolate);
                    (void) arr->Set(context, idx++, elem);
                    return true;
                });
                _rst = arr;
            }

            template <typename T>
            static void list_ordinary(const METADAT& _dat, Local<Value>& _rst, Isolate* _isolate) {
                const BIT8 *ptr = _dat.ptr, *end = _dat.ptr + _dat.size;
                size_t count = _dat.size / sizeof(T);
                Local<Array> arr = Array::New(_isolate, (int) count);
                Local<Context> context = _isolate->GetCurrentContext();
                for (uint32_t i = 0; i < count && ptr + sizeof(T) <= end; i++) {
                    METADAT elem{__stack_only_type<T>, (BIT32) sizeof(T), {nullptr, 0}, ptr};
                    Local<Value> val;
                    read_dat(elem, val, _isolate);
                    (void) arr->Set(context, i, val);
                    ptr += sizeof(T);
                }
                _rst = arr;
            }
            static void list_bytes(const METADAT& _dat, Local<Value>& _rst, Isolate* _isolate) {
                Local<Array> arr = Array::New(_isolate);
                Local<Context> context = _isolate->GetCurrentContext();
                uint32_t idx = 0;
                deserer<varmap, variant>::for_each_meta(_dat.ptr, _dat.ptr + _dat.size, [&](const METADAT& meta) {
                    Local<Value> elem;
                    if (_bytes_ == (TYPE) meta.type) base_bytes(meta, elem, _isolate);
                    (void) arr->Set(context, idx++, elem);
                    return true;
                });
                _rst = arr;
            }
            static void list_string(const METADAT& _dat, Local<Value>& _rst, Isolate* _isolate) {
                Local<Array> arr = Array::New(_isolate);
                Local<Context> context = _isolate->GetCurrentContext();
                uint32_t idx = 0;
                deserer<varmap, variant>::for_each_meta(_dat.ptr, _dat.ptr + _dat.size, [&](const METADAT& meta) {
                    Local<Value> elem;
                    base_string(meta, elem, _isolate);
                    (void) arr->Set(context, idx++, elem);
                    return true;
                });
                _rst = arr;
            }
            static void list_any(const METADAT& _dat, Local<Value>& _rst, Isolate* _isolate) {
                Local<Array> arr = Array::New(_isolate);
                Local<Context> context = _isolate->GetCurrentContext();
                uint32_t idx = 0;
                deserer<varmap, variant>::for_each_meta(_dat.ptr, _dat.ptr + _dat.size, [&](const METADAT& meta) {
                    Local<Value> elem;
                    read_dat(meta, elem, _isolate);
                    (void) arr->Set(context, idx++, elem);
                    return true;
                });
                _rst = arr;
            }

        public:
            static void read_dat(const METADAT& _dat, Local<Value>& _rst, Isolate* _isolate) {
                if ((_dat.type & 0X0000FFFFU) == _dat.type) {
                    if (_varmap_ == (TYPE) _dat.type) read_map(_dat, _rst, _isolate);
                    else if (_void_ == (TYPE) _dat.type) _rst = Null(_isolate);
                    else {
                        auto it = CORE_BASE.find((BIT16) _dat.type);
                        if (it != CORE_BASE.end()) it->second(_dat, _rst, _isolate);
                        else _rst = Local<Value>();
                    }
                } else if ((_dat.type & 0X0000FFFFU) == _vector_) {
                    auto it = CORE__VEC.find((BIT16) (_dat.type >> 16));
                    if (it != CORE__VEC.end()) it->second(_dat, _rst, _isolate);
                    else _rst = Local<Value>();
                } else if ((_dat.type & 0X0000FFFFU) == _list_) {
                    auto it = CORE_LIST.find((BIT16) (_dat.type >> 16));
                    if (it != CORE_LIST.end()) it->second(_dat, _rst, _isolate);
                    else _rst = Local<Value>();
                } else _rst = Local<Value>();
            }

            static void read_map(const METADAT& _dat, Local<Value>& _rst, Isolate* _isolate) {
                Local<Object> obj = Object::New(_isolate);
                Local<Context> context = _isolate->GetCurrentContext();
                deserer<varmap, variant>::for_each_meta(_dat.ptr, _dat.ptr + _dat.size, [&](const METADAT& meta) {
                    Local<Value> val;
                    read_dat(meta, val, _isolate);
                    if (!val.IsEmpty()) {
                        std::string key(meta.name.data, meta.name.size);
                        (void) obj->Set(context,
                                        String::NewFromUtf8(_isolate, key.c_str()).ToLocalChecked(),
                                        val);
                    }
                    return true;
                });
                _rst = obj;
            }

            static bool init_core() {
                static bool is_init = []() {
                    CORE_BASE = std::unordered_map<BIT16, COREFUNC>{
                        {_bool_, &deserer_v8::base_bool},
                        {_int8_, &deserer_v8::base_int_like<int_8>},
                        {_int16_, &deserer_v8::base_int_like<int_16>},
                        {_int32_, &deserer_v8::base_int_like<int_32>},
                        {_int64_, &deserer_v8::base_int64},
                        {_uint8_, &deserer_v8::base_uint_like<uint_8>},
                        {_uint16_, &deserer_v8::base_uint_like<uint_16>},
                        {_uint32_, &deserer_v8::base_uint_like<uint_32>},
                        {_uint64_, &deserer_v8::base_uint64},
                        {_real32_, &deserer_v8::base_real32},
                        {_real64_, &deserer_v8::base_real64},
                        {_bytes_, &deserer_v8::base_bytes},
                        {_string_, &deserer_v8::base_string},
                    };
                    CORE__VEC = std::unordered_map<BIT16, COREFUNC>{
                        {_bool_, &deserer_v8::vec_bool},
                        {_int8_, &deserer_v8::vec_typed<v8::Int8Array, int_8>},
                        {_int16_, &deserer_v8::vec_typed<v8::Int16Array, int_16>},
                        {_int32_, &deserer_v8::vec_typed<v8::Int32Array, int_32>},
                        {_int64_, &deserer_v8::vec_typed<v8::BigInt64Array, int_64>},
                        {_uint8_, &deserer_v8::vec_typed<v8::Uint8Array, uint_8>},
                        {_uint16_, &deserer_v8::vec_typed<v8::Uint16Array, uint_16>},
                        {_uint32_, &deserer_v8::vec_typed<v8::Uint32Array, uint_32>},
                        {_uint64_, &deserer_v8::vec_typed<v8::BigUint64Array, uint_64>},
                        {_real32_, &deserer_v8::vec_typed<v8::Float32Array, real_32>},
                        {_real64_, &deserer_v8::vec_typed<v8::Float64Array, real_64>},
                        {_bytes_, &deserer_v8::vec_bytes},
                        {_string_, &deserer_v8::vec_string},
                        {_any_, &deserer_v8::vec_any},
                    };
                    CORE_LIST = std::unordered_map<BIT16, COREFUNC>{
                        {_bool_, &deserer_v8::list_ordinary<bool>},
                        {_int8_, &deserer_v8::list_ordinary<int_8>},
                        {_int16_, &deserer_v8::list_ordinary<int_16>},
                        {_int32_, &deserer_v8::list_ordinary<int_32>},
                        {_int64_, &deserer_v8::list_ordinary<int_64>},
                        {_uint8_, &deserer_v8::list_ordinary<uint_8>},
                        {_uint16_, &deserer_v8::list_ordinary<uint_16>},
                        {_uint32_, &deserer_v8::list_ordinary<uint_32>},
                        {_uint64_, &deserer_v8::list_ordinary<uint_64>},
                        {_real32_, &deserer_v8::list_ordinary<real_32>},
                        {_real64_, &deserer_v8::list_ordinary<real_64>},
                        {_bytes_, &deserer_v8::list_bytes},
                        {_string_, &deserer_v8::list_string},
                        {_any_, &deserer_v8::list_any},
                    };
                    return true;
                }();
                return is_init;
            }
        };

        std::unordered_map<BIT16, deserer_v8::COREFUNC> deserer_v8::CORE_BASE;
        std::unordered_map<BIT16, deserer_v8::COREFUNC> deserer_v8::CORE__VEC;
        std::unordered_map<BIT16, deserer_v8::COREFUNC> deserer_v8::CORE_LIST;

        class serer_v8 {
        private:

            class s_void : public serable {
            public:
                s_void(const std::string& _name = "") : serable(_void_, _name) {}

            protected:
                virtual BIT64 extend_size() const override { return 0; }
                virtual BIT8* extend_read(BIT8* _ptr) const override { return _ptr; }
                virtual bool extend_read(ostream& _stm) const override { return true; }
            };

            template <typename T>
            static serable* typed_array_to_serable(const std::string& _name, Local<v8::ArrayBufferView> _view) {
                size_t len = _view->ByteLength() / sizeof(T);
                std::vector<T> data(len);
                auto backing = _view->Buffer()->GetBackingStore();
                const char* src = static_cast<const char*>(backing->Data()) + _view->ByteOffset();
                memcpy(data.data(), src, len * sizeof(T));
                return new typename __s_vector<T>::value(std::move(data), _name);
            }

        public:

            static serable* v8_to_serable(const std::string& _name, Local<Value> _val, Isolate* _isolate) {

                if (_val->IsBoolean())
                    return new s_bool(_val.As<Boolean>()->Value(), _name);

                if (_val->IsInt32())
                    return new s_int_32(_val.As<v8::Int32>()->Value(), _name);

                if (_val->IsUint32())
                    return new s_uint_32(_val.As<v8::Uint32>()->Value(), _name);

                if (_val->IsBigInt()) {
                    Local<v8::BigInt> bi = _val.As<v8::BigInt>();
                    bool lossless = false;
                    int_64 i64 = bi->Int64Value(&lossless);
                    if (lossless) return new s_int_64(i64, _name);
                    uint_64 ui64 = bi->Uint64Value(&lossless);
                    if (lossless) return new s_uint_64(ui64, _name);
                    String::Utf8Value str(_isolate, bi->ToString(_isolate->GetCurrentContext()).ToLocalChecked());
                    return new s_string(std::string(*str, str.length()), _name);
                }

                if (_val->IsNumber())
                    return new s_real_64(_val.As<Number>()->Value(), _name);

                if (_val->IsString()) {
                    String::Utf8Value str(_isolate, _val);
                    return new s_string(std::string(*str, str.length()), _name);
                }

                if (_val->IsNull())
                    return new s_void(_name);

                if (_val->IsArrayBuffer()) {
                    Local<ArrayBuffer> buf = _val.As<ArrayBuffer>();
                    auto backing = buf->GetBackingStore();
                    return new s_bytes(bytes((BIT8*) backing->Data(), backing->ByteLength()), _name);
                }

                if (_val->IsArray()) {
                    Local<Array> arr = _val.As<Array>();
                    Local<Context> ctx = _isolate->GetCurrentContext();
                    s_varvec* vec = new s_varvec(_name);
                    for (uint32_t i = 0; i < arr->Length(); i++) {
                        serable* child = v8_to_serable("", arr->Get(ctx, i).ToLocalChecked(), _isolate);
                        if (!child) {
                            delete vec;
                            return nullptr;
                        }
                        vec->append(child);
                    }
                    return vec;
                }

#define MAP_TA(V8NAME, CTYPE) \
    if (_val->Is##V8NAME()) return typed_array_to_serable<CTYPE>(_name, _val.As<v8::V8NAME>());

                MAP_TA(Int8Array, int_8);
                MAP_TA(Uint8Array, uint_8);
                MAP_TA(Uint8ClampedArray, uint_8);
                MAP_TA(Int16Array, int_16);
                MAP_TA(Uint16Array, uint_16);
                MAP_TA(Int32Array, int_32);
                MAP_TA(Uint32Array, uint_32);
                MAP_TA(Float32Array, real_32);
                MAP_TA(Float64Array, real_64);
                MAP_TA(BigInt64Array, int_64);
                MAP_TA(BigUint64Array, uint_64);
#undef MAP_TA

                if (_val->IsObject()) {
                    Local<Object> obj = _val.As<Object>();
                    Local<Context> ctx = _isolate->GetCurrentContext();
                    Local<Array> keys = obj->GetPropertyNames(ctx).ToLocalChecked();
                    s_varmap* map = new s_varmap(_name);
                    for (uint32_t i = 0; i < keys->Length(); i++) {
                        Local<Value> key = keys->Get(ctx, i).ToLocalChecked();
                        if (!key->IsString()) continue;
                        String::Utf8Value keyStr(_isolate, key);
                        std::string key_name(*keyStr, keyStr.length());
                        Local<Value> child = obj->Get(ctx, key).ToLocalChecked();
                        serable* c = v8_to_serable(key_name, child, _isolate);
                        if (!c) {
                            delete map;
                            return nullptr;
                        }
                        map->append(c);
                    }
                    return map;
                }

                return nullptr;
            }
        };
    }
}

using namespace alx::ser;

Local<Object> alx::varsolid_node::to_object(const bytes_view& _bytes, Isolate* _isolate) {
    deserer_v8::init_core();
    Local<Object> result = Object::New(_isolate);
    Local<Context> context = _isolate->GetCurrentContext();
    deserer<varmap, variant>::for_each_meta(_bytes, [&](const deserer_v8::METADAT& meta) {
        Local<Value> val;
        deserer_v8::read_dat(meta, val, _isolate);
        if (!val.IsEmpty()) {
            std::string key(meta.name.data, meta.name.size);
            (void) result->Set(context,
                               String::NewFromUtf8(_isolate, key.c_str()).ToLocalChecked(),
                               val);
        }
        return true;
    });
    return result;
}

Local<Value> alx::varsolid_node::to_value(const bytes_view& _bytes, Isolate* _isolate) {
    deserer_v8::init_core();
    if (!deserer<varmap, variant>::valid(_bytes)) return Local<Value>();

    deserer_v8::METADAT item;
    item.type = _varmap_;
    item.ptr = _bytes.data() + serer::__HEAD_SIZE__;
    item.size = _bytes.to<serer::HEAD>().SIZE;
    item.name = {nullptr, 0};

    if (item.size == 0) return Local<Value>();

    Local<Value> result;
    deserer_v8::read_dat(item, result, _isolate);
    return result;
}

Local<Value> alx::varsolid_node::get_value(const bytes_view& _bytes, const std::list<std::string>& _path, Isolate* _isolate, const Local<Value>& _def) {
    deserer_v8::init_core();
    if (!deserer<varmap, variant>::valid(_bytes)) return _def;
    if (_path.empty()) return to_object(_bytes, _isolate);

    serer::HEAD head = _bytes.to<serer::HEAD>();
    deserer_v8::METADAT item;
    item.type = _varmap_;
    item.ptr = _bytes.data() + serer::__HEAD_SIZE__;
    item.size = head.SIZE;
    item.name = {nullptr, 0};

    for (const std::string& key : _path) {
        if (_varmap_ == (TYPE) item.type) {
            deserer_v8::METADAT found{};
            bool has_found{false};
            deserer<varmap, variant>::for_each_meta(item.ptr, item.ptr + item.size, [&](const deserer_v8::METADAT& meta) {
                if (meta.name == key) {
                    found = meta;
                    has_found = true;
                    return false;
                }
                return true;
            });
            if (has_found) item = found;
            else return _def;
        } else if ((BIT32(_any_ << 16 | _vector_) == item.type || BIT32(_any_ << 16 | _list_) == item.type) &&
                   std::all_of(key.begin(), key.end(), ::isdigit)) {
            uint_64 index = std::stoull(key);
            deserer_v8::METADAT found{};
            bool has_found{false};
            uint_64 idx{0};
            deserer<varmap, variant>::for_each_meta(item.ptr, item.ptr + item.size, [&](const deserer_v8::METADAT& meta) {
                if (idx++ == index) {
                    found = meta;
                    has_found = true;
                    return false;
                }
                return true;
            });
            if (has_found) item = found;
            else return _def;
        } else return _def;
    }

    Local<Value> result;
    deserer_v8::read_dat(item, result, _isolate);
    return result.IsEmpty() ? _def : result;
}

bytes alx::varsolid_node::to_bytes(Local<Object> _obj, Isolate* _isolate) {
    Local<Context> ctx = _isolate->GetCurrentContext();
    Local<Array> keys = _obj->GetPropertyNames(ctx).ToLocalChecked();

    serer sr;
    for (uint32_t i = 0; i < keys->Length(); i++) {
        Local<Value> key = keys->Get(ctx, i).ToLocalChecked();
        if (!key->IsString()) continue;
        String::Utf8Value keyStr(_isolate, key);
        std::string key_name(*keyStr, keyStr.length());
        Local<Value> child = _obj->Get(ctx, key).ToLocalChecked();
        serable* s = serer_v8::v8_to_serable(key_name, child, _isolate);
        if (!s) return bytes();
        sr.append(s);
    }

    return sr.read();
}
