/*****************************************************************/ /**
 * \file   varsolid_py.cpp
 * \brief  Python binding — direct binary ↔ Python object serialization
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "varsolid_py.h"
#include "aserial.h"
#include <algorithm>
#include <cstring>
#include <list>
#include <unordered_map>
#include <vector>

using namespace alx;
using namespace alx::ser;

namespace alx {
    namespace varsolid_py {

        class deserer_py {
        public:
            typedef deserer<varmap, variant>::METADAT METADAT;
            typedef void (*COREFUNC)(const METADAT&, py::object&);

            static std::unordered_map<BIT16, COREFUNC> CORE_BASE;
            static std::unordered_map<BIT16, COREFUNC> CORE__VEC;
            static std::unordered_map<BIT16, COREFUNC> CORE_LIST;

        private:

            static py::bytes make_bytes(const void* _data, size_t _size) {
                return py::bytes((const char*) _data, _size);
            }

            static py::bytes make_typed_bytes(const BIT8* _data, BIT32 _size) {
                return py::bytes((const char*) _data, _size);
            }

            template <typename T>
            static void base_int_like(const METADAT& _dat, py::object& _rst) {
                _rst = py::int_(static_cast<int32_t>(r_interpret<T>(_dat.ptr)));
            }
            template <typename T>
            static void base_uint_like(const METADAT& _dat, py::object& _rst) {
                _rst = py::int_(static_cast<uint32_t>(r_interpret<T>(_dat.ptr)));
            }
            static void base_bool(const METADAT& _dat, py::object& _rst) {
                _rst = py::bool_(r_interpret<bool>(_dat.ptr));
            }
            static void base_int64(const METADAT& _dat, py::object& _rst) {
                _rst = py::int_(r_interpret<int_64>(_dat.ptr));
            }
            static void base_uint64(const METADAT& _dat, py::object& _rst) {
                _rst = py::int_(r_interpret<uint_64>(_dat.ptr));
            }
            static void base_real32(const METADAT& _dat, py::object& _rst) {
                _rst = py::float_(r_interpret<real_32>(_dat.ptr));
            }
            static void base_real64(const METADAT& _dat, py::object& _rst) {
                _rst = py::float_(r_interpret<real_64>(_dat.ptr));
            }
            static void base_string(const METADAT& _dat, py::object& _rst) {
                _rst = py::str((const char*) _dat.ptr, _dat.size);
            }
            static void base_bytes(const METADAT& _dat, py::object& _rst) {
                _rst = make_bytes(_dat.ptr, _dat.size);
            }

            template <typename T>
            static void vec_typed(const METADAT& _dat, py::object& _rst) {
                size_t count = _dat.size / sizeof(T);
                py::list arr(count);
                const T* ptr = reinterpret_cast<const T*>(_dat.ptr);
                for (size_t i = 0; i < count; i++)
                    arr[i] = py::cast(ptr[i]);
                _rst = std::move(arr);
            }
            static void vec_bool(const METADAT& _dat, py::object& _rst) {
                size_t count = _dat.size / sizeof(bool);
                py::list arr(count);
                const bool* ptr = reinterpret_cast<const bool*>(_dat.ptr);
                for (size_t i = 0; i < count; i++)
                    arr[i] = py::bool_(ptr[i]);
                _rst = std::move(arr);
            }
            static void vec_bytes(const METADAT& _dat, py::object& _rst) {
                py::list arr;
                deserer<varmap, variant>::for_each_meta(_dat.ptr, _dat.ptr + _dat.size, [&](const METADAT& meta) {
                    if (_bytes_ == (TYPE) meta.type) arr.append(make_bytes(meta.ptr, meta.size));
                    return true;
                });
                _rst = std::move(arr);
            }
            static void vec_string(const METADAT& _dat, py::object& _rst) {
                py::list arr;
                deserer<varmap, variant>::for_each_meta(_dat.ptr, _dat.ptr + _dat.size, [&](const METADAT& meta) {
                    arr.append(py::str((const char*) meta.ptr, meta.size));
                    return true;
                });
                _rst = std::move(arr);
            }
            static void vec_any(const METADAT& _dat, py::object& _rst) {
                py::list arr;
                deserer<varmap, variant>::for_each_meta(_dat.ptr, _dat.ptr + _dat.size, [&](const METADAT& meta) {
                    py::object elem;
                    read_dat(meta, elem);
                    arr.append(std::move(elem));
                    return true;
                });
                _rst = std::move(arr);
            }

            template <typename T>
            static void list_ordinary(const METADAT& _dat, py::object& _rst) {
                const BIT8* ptr = _dat.ptr;
                const BIT8* end = _dat.ptr + _dat.size;
                size_t count = _dat.size / sizeof(T);
                py::list arr;
                for (size_t i = 0; i < count && ptr + sizeof(T) <= end; i++) {
                    METADAT elem{__stack_only_type<T>, (BIT32) sizeof(T), {nullptr, 0}, ptr};
                    py::object val;
                    read_dat(elem, val);
                    arr.append(std::move(val));
                    ptr += sizeof(T);
                }
                _rst = std::move(arr);
            }
            static void list_bytes(const METADAT& _dat, py::object& _rst) {
                py::list arr;
                deserer<varmap, variant>::for_each_meta(_dat.ptr, _dat.ptr + _dat.size, [&](const METADAT& meta) {
                    if (_bytes_ == (TYPE) meta.type) arr.append(make_bytes(meta.ptr, meta.size));
                    return true;
                });
                _rst = std::move(arr);
            }
            static void list_string(const METADAT& _dat, py::object& _rst) {
                py::list arr;
                deserer<varmap, variant>::for_each_meta(_dat.ptr, _dat.ptr + _dat.size, [&](const METADAT& meta) {
                    arr.append(py::str((const char*) meta.ptr, meta.size));
                    return true;
                });
                _rst = std::move(arr);
            }
            static void list_any(const METADAT& _dat, py::object& _rst) {
                py::list arr;
                deserer<varmap, variant>::for_each_meta(_dat.ptr, _dat.ptr + _dat.size, [&](const METADAT& meta) {
                    py::object elem;
                    read_dat(meta, elem);
                    arr.append(std::move(elem));
                    return true;
                });
                _rst = std::move(arr);
            }

        public:
            static void read_dat(const METADAT& _dat, py::object& _rst) {
                if ((_dat.type & 0x0000FFFFU) == _dat.type) {
                    if (_varmap_ == (TYPE) _dat.type) read_map(_dat, _rst);
                    else if (_void_ == (TYPE) _dat.type) _rst = py::none();
                    else {
                        auto it = CORE_BASE.find((BIT16) _dat.type);
                        if (it != CORE_BASE.end()) it->second(_dat, _rst);
                        else _rst = py::none();
                    }
                } else if ((_dat.type & 0x0000FFFFU) == _vector_) {
                    auto it = CORE__VEC.find((BIT16) (_dat.type >> 16));
                    if (it != CORE__VEC.end()) it->second(_dat, _rst);
                    else _rst = py::none();
                } else if ((_dat.type & 0x0000FFFFU) == _list_) {
                    auto it = CORE_LIST.find((BIT16) (_dat.type >> 16));
                    if (it != CORE_LIST.end()) it->second(_dat, _rst);
                    else _rst = py::none();
                } else _rst = py::none();
            }

            static void read_map(const METADAT& _dat, py::object& _rst) {
                py::dict obj;
                deserer<varmap, variant>::for_each_meta(_dat.ptr, _dat.ptr + _dat.size, [&](const METADAT& meta) {
                    py::object val;
                    read_dat(meta, val);
                    if (!val.is_none() || _void_ == (TYPE) (meta.type & 0x0000FFFFU)) {
                        obj[py::str(meta.name.data, meta.name.size)] = std::move(val);
                    }
                    return true;
                });
                _rst = std::move(obj);
            }

            static bool init_core() {
                static bool is_init = []() {
                    CORE_BASE = std::unordered_map<BIT16, COREFUNC>{
                        {_bool_, &deserer_py::base_bool},
                        {_int8_, &deserer_py::base_int_like<int_8>},
                        {_int16_, &deserer_py::base_int_like<int_16>},
                        {_int32_, &deserer_py::base_int_like<int_32>},
                        {_int64_, &deserer_py::base_int64},
                        {_uint8_, &deserer_py::base_uint_like<uint_8>},
                        {_uint16_, &deserer_py::base_uint_like<uint_16>},
                        {_uint32_, &deserer_py::base_uint_like<uint_32>},
                        {_uint64_, &deserer_py::base_uint64},
                        {_real32_, &deserer_py::base_real32},
                        {_real64_, &deserer_py::base_real64},
                        {_string_, &deserer_py::base_string},
                        {_bytes_, &deserer_py::base_bytes},
                    };
                    CORE__VEC = std::unordered_map<BIT16, COREFUNC>{
                        {_bool_, &deserer_py::vec_bool},
                        {_int8_, &deserer_py::vec_typed<int_8>},
                        {_int16_, &deserer_py::vec_typed<int_16>},
                        {_int32_, &deserer_py::vec_typed<int_32>},
                        {_int64_, &deserer_py::vec_typed<int_64>},
                        {_uint8_, &deserer_py::vec_typed<uint_8>},
                        {_uint16_, &deserer_py::vec_typed<uint_16>},
                        {_uint32_, &deserer_py::vec_typed<uint_32>},
                        {_uint64_, &deserer_py::vec_typed<uint_64>},
                        {_real32_, &deserer_py::vec_typed<real_32>},
                        {_real64_, &deserer_py::vec_typed<real_64>},
                        {_bytes_, &deserer_py::vec_bytes},
                        {_string_, &deserer_py::vec_string},
                        {_any_, &deserer_py::vec_any},
                    };
                    CORE_LIST = std::unordered_map<BIT16, COREFUNC>{
                        {_bool_, &deserer_py::list_ordinary<bool>},
                        {_int8_, &deserer_py::list_ordinary<int_8>},
                        {_int16_, &deserer_py::list_ordinary<int_16>},
                        {_int32_, &deserer_py::list_ordinary<int_32>},
                        {_int64_, &deserer_py::list_ordinary<int_64>},
                        {_uint8_, &deserer_py::list_ordinary<uint_8>},
                        {_uint16_, &deserer_py::list_ordinary<uint_16>},
                        {_uint32_, &deserer_py::list_ordinary<uint_32>},
                        {_uint64_, &deserer_py::list_ordinary<uint_64>},
                        {_real32_, &deserer_py::list_ordinary<real_32>},
                        {_real64_, &deserer_py::list_ordinary<real_64>},
                        {_bytes_, &deserer_py::list_bytes},
                        {_string_, &deserer_py::list_string},
                        {_any_, &deserer_py::list_any},
                    };
                    return true;
                }();
                return is_init;
            }
        };

        std::unordered_map<BIT16, deserer_py::COREFUNC> deserer_py::CORE_BASE;
        std::unordered_map<BIT16, deserer_py::COREFUNC> deserer_py::CORE__VEC;
        std::unordered_map<BIT16, deserer_py::COREFUNC> deserer_py::CORE_LIST;

        class serer_py {
        private:
            class s_void : public serable {
            public:
                s_void(const std::string& _name = "") : serable(_void_, _name) {}

            protected:
                virtual BIT64 extend_size() const override { return 0; }
                virtual BIT8* extend_read(BIT8* _ptr) const override { return _ptr; }
                virtual bool extend_read(ostream& _stm) const override { return true; }
            };

        public:

            template <typename T>
            static bool extract_vec(const py::handle& _val, std::vector<T>& out) {
                PyObject* seq = _val.ptr();
                Py_ssize_t len;
                if (PyList_Check(seq)) {
                    len = PyList_Size(seq);
                    out.clear();
                    out.reserve(len);
                    for (Py_ssize_t i = 0; i < len; i++) {
                        PyObject* item = PyList_GetItem(seq, i);
                        T val;
                        if (!extract_elem(item, val)) return false;
                        out.push_back(std::move(val));
                    }
                } else if (PyTuple_Check(seq)) {
                    len = PyTuple_Size(seq);
                    out.clear();
                    out.reserve(len);
                    for (Py_ssize_t i = 0; i < len; i++) {
                        PyObject* item = PyTuple_GetItem(seq, i);
                        T val;
                        if (!extract_elem(item, val)) return false;
                        out.push_back(std::move(val));
                    }
                } else return false;
                return true;
            }

            static bool extract_elem(PyObject* _obj, int_32& _val) {
                if (!PyLong_Check(_obj)) return false;
                int overflow;
                long val = PyLong_AsLongAndOverflow(_obj, &overflow);
                if (overflow || val < INT32_MIN || val > INT32_MAX) return false;
                _val = (int_32) val;
                return true;
            }
            static bool extract_elem(PyObject* _obj, int_64& _val) {
                if (!PyLong_Check(_obj)) return false;
                int overflow;
                int64_t val = (int64_t) PyLong_AsLongLongAndOverflow(_obj, &overflow);
                if (overflow) return false;
                _val = val;
                return true;
            }
            static bool extract_elem(PyObject* _obj, uint_64& _val) {
                if (!PyLong_Check(_obj)) return false;
                unsigned long long val = PyLong_AsUnsignedLongLong(_obj);
                if (val == (unsigned long long) -1 && PyErr_Occurred()) {
                    PyErr_Clear();
                    return false;
                }
                _val = val;
                return true;
            }
            static bool extract_elem(PyObject* _obj, real_64& _val) {
                if (PyLong_Check(_obj)) {
                    _val = (real_64) PyLong_AsDouble(_obj);
                    return true;
                }
                if (PyFloat_Check(_obj)) {
                    _val = PyFloat_AsDouble(_obj);
                    return true;
                }
                return false;
            }
            static bool extract_elem(PyObject* _obj, bool& _val) {
                if (!PyBool_Check(_obj)) return false;
                _val = (_obj == Py_True);
                return true;
            }

            template <typename T>
            static serable* try_typed_vec(const std::string& _name, const py::handle& _val) {
                std::vector<T> data;
                if (!extract_vec<T>(_val, data)) return nullptr;
                return new typename __s_vector<T>::value(std::move(data), _name);
            }

            static serable* py_to_serable(const std::string& _name, const py::handle& _val) {

                if (_val.is_none())
                    return new s_void(_name);

                if (py::isinstance<py::bool_>(_val))
                    return new s_bool(_val.cast<bool>(), _name);

                if (py::isinstance<py::int_>(_val)) {
                    try {
                        int64_t i64 = _val.cast<int64_t>();
                        if (i64 >= INT32_MIN && i64 <= INT32_MAX)
                            return new s_int_32((int_32) i64, _name);
                        if (i64 >= 0 && i64 <= UINT32_MAX)
                            return new s_uint_32((uint_32) i64, _name);
                        return new s_int_64(i64, _name);
                    } catch (...) {
                        try {
                            uint64_t ui64 = _val.cast<uint64_t>();
                            return new s_uint_64(ui64, _name);
                        } catch (...) {

                            std::string str = py::cast<std::string>(_val.attr("__str__")());
                            return new s_string(str, _name);
                        }
                    }
                }

                if (py::isinstance<py::float_>(_val))
                    return new s_real_64(_val.cast<real_64>(), _name);

                if (py::isinstance<py::str>(_val))
                    return new s_string(_val.cast<std::string>(), _name);

                if (py::isinstance<py::bytes>(_val)) {
                    std::string data = _val.cast<std::string>();
                    return new s_bytes(bytes((const BIT8*) data.data(), data.size()), _name);
                }
                if (PyByteArray_Check(_val.ptr())) {
                    char* buf = PyByteArray_AsString(_val.ptr());
                    Py_ssize_t len = PyByteArray_Size(_val.ptr());
                    return new s_bytes(bytes((const BIT8*) buf, (size_t) len), _name);
                }

                if (PyMemoryView_Check(_val.ptr())) {
                    Py_buffer* view = PyMemoryView_GET_BUFFER(_val.ptr());
                    return new s_bytes(bytes((const BIT8*) view->buf, view->len), _name);
                }

                if (py::isinstance<py::list>(_val) || py::isinstance<py::tuple>(_val)) {
                    size_t len = py::len(_val);

                    if (len >= 16 && len > 0) {
                        auto first = _val[py::int_(0)];
                        serable* vec = nullptr;
                        if (py::isinstance<py::int_>(first)) {

                            vec = try_typed_vec<int_32>(_name, _val);
                            if (!vec) vec = try_typed_vec<int_64>(_name, _val);
                            if (!vec) vec = try_typed_vec<uint_64>(_name, _val);
                        } else if (py::isinstance<py::float_>(first)) {
                            vec = try_typed_vec<real_64>(_name, _val);
                        } else if (py::isinstance<py::bool_>(first)) {
                            vec = try_typed_vec<bool>(_name, _val);
                        }
                        if (vec) return vec;
                    }

                    s_varvec* vec = new s_varvec(_name);
                    for (auto item : _val) {
                        serable* child = py_to_serable("", item);
                        if (!child) {
                            delete vec;
                            return nullptr;
                        }
                        vec->append(child);
                    }
                    return vec;
                }

                if (py::isinstance<py::dict>(_val)) {
                    s_varmap* map = new s_varmap(_name);
                    py::dict d = _val.cast<py::dict>();
                    for (auto item : d) {
                        if (!py::isinstance<py::str>(item.first)) continue;
                        std::string key = item.first.cast<std::string>();
                        serable* child = py_to_serable(key, item.second);
                        if (!child) {
                            delete map;
                            return nullptr;
                        }
                        map->append(child);
                    }
                    return map;
                }

                return nullptr;
            }
        };
    }
}

using namespace alx::ser;

py::object alx::varsolid_py::to_object(const bytes_view& _bytes) {
    deserer_py::init_core();
    py::dict result;
    deserer<varmap, variant>::for_each_meta(_bytes, [&](const deserer_py::METADAT& meta) {
        py::object val;
        deserer_py::read_dat(meta, val);
        if (!val.is_none() || _void_ == (TYPE) (meta.type & 0x0000FFFFU)) {
            result[py::str(meta.name.data, meta.name.size)] = std::move(val);
        }
        return true;
    });
    return std::move(result);
}

py::object alx::varsolid_py::to_value(const bytes_view& _bytes) {
    deserer_py::init_core();
    if (!deserer<varmap, variant>::valid(_bytes)) return py::none();

    deserer_py::METADAT item;
    item.type = _varmap_;
    item.ptr = _bytes.data() + serer::__HEAD_SIZE__;
    item.size = _bytes.to<serer::HEAD>().SIZE;
    item.name = {nullptr, 0};

    if (item.size == 0) return py::none();

    py::object result;
    deserer_py::read_dat(item, result);
    return result;
}

py::object alx::varsolid_py::get_value(const bytes_view& _bytes, const std::list<std::string>& _path, const py::object& _def) {
    deserer_py::init_core();
    if (!deserer<varmap, variant>::valid(_bytes)) return _def;
    if (_path.empty()) return to_object(_bytes);

    serer::HEAD head = _bytes.to<serer::HEAD>();
    deserer_py::METADAT item;
    item.type = _varmap_;
    item.ptr = _bytes.data() + serer::__HEAD_SIZE__;
    item.size = head.SIZE;
    item.name = {nullptr, 0};

    for (const std::string& key : _path) {
        if (_varmap_ == (TYPE) item.type) {
            deserer_py::METADAT found{};
            bool has_found{false};
            deserer<varmap, variant>::for_each_meta(item.ptr, item.ptr + item.size, [&](const deserer_py::METADAT& meta) {
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
            deserer_py::METADAT found{};
            bool has_found{false};
            uint_64 idx{0};
            deserer<varmap, variant>::for_each_meta(item.ptr, item.ptr + item.size, [&](const deserer_py::METADAT& meta) {
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

    py::object result;
    deserer_py::read_dat(item, result);
    return result.is_none() && _void_ != (TYPE) (item.type & 0x0000FFFFU) ? _def : result;
}

bytes alx::varsolid_py::to_bytes(py::dict _obj) {
    serer sr;
    for (auto item : _obj) {
        if (!py::isinstance<py::str>(item.first)) continue;
        std::string key = item.first.cast<std::string>();
        serable* s = serer_py::py_to_serable(key, item.second);
        if (!s) return bytes();
        sr.append(s);
    }
    return sr.read();
}
