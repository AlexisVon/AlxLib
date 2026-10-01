/*****************************************************************/ /**
 * \file   avarsolid.cpp
 * \brief  Serialization and deserialization of varmap
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "avarsolid.h"
#include "aserial.h"
#include <vector>

using namespace alx;
using namespace alx::ser;

namespace alx {
    namespace varsolid {
        // whole-node size (payload + overhead + name), and never 0: 0 is the failure sentinel
        BIT64 reader_variant_size(uint_64 _nsize, const variant* _var) {
#define INSTALL_BASE_TYPE(VTYPE, RTYPE) \
    case variant::id<VTYPE>(): data_size = RTYPE::sdata_size(_var->to<VTYPE>()); break;
#define INSTALL_TYPE(VTYPE, RTYPE)                          \
    INSTALL_BASE_TYPE(VTYPE, RTYPE);                        \
    INSTALL_BASE_TYPE(std::vector<VTYPE>, s_vector<VTYPE>); \
    INSTALL_BASE_TYPE(std::list<VTYPE>, s_list<VTYPE>);
#define INSTALL_CPTR_TYPE(VTYPE, RTYPE, CTYPE)                          \
    case variant::id<CTYPE<VTYPE>>(): {                                 \
        for (const VTYPE& it : _var->to<CTYPE<VTYPE>>()) {              \
            data_size += (serable::EMPTY_SIZE + RTYPE::sdata_size(it)); \
        }                                                               \
        break;                                                          \
    }

            if (_nsize > _MAX_NAME__) return 0;

            BIT64 data_size{0};
            switch (_var->type()) {
                INSTALL_TYPE(bool, s_bool);
                INSTALL_TYPE(int_8, s_int_8);
                INSTALL_TYPE(int_16, s_int_16);
                INSTALL_TYPE(int_32, s_int_32);
                INSTALL_TYPE(int_64, s_int_64);
                INSTALL_TYPE(uint_8, s_uint_8);
                INSTALL_TYPE(uint_16, s_uint_16);
                INSTALL_TYPE(uint_32, s_uint_32);
                INSTALL_TYPE(uint_64, s_uint_64);
                INSTALL_TYPE(real_32, s_real_32);
                INSTALL_TYPE(real_64, s_real_64);

                INSTALL_BASE_TYPE(bytes, s_bytes);
                INSTALL_CPTR_TYPE(bytes, s_bytes, std::vector);
                INSTALL_CPTR_TYPE(bytes, s_bytes, std::list);

                INSTALL_BASE_TYPE(std::string, s_string);
                INSTALL_CPTR_TYPE(std::string, s_string, std::vector);
                INSTALL_CPTR_TYPE(std::string, s_string, std::list);

            case variant::id<varmap>(): {
                BIT64 size;
                for (const auto& it : _var->to<varmap>()) {
                    size = reader_variant_size(it.first.size(), it.second);
                    // one failing child fails the whole node; the 0 does not say which reason
                    if (0 == size) return 0;
                    else data_size += size;
                }
                break;
            }

            case variant::id<varvec>(): {
                BIT64 size;
                for (const auto& it : _var->to<varvec>()) {
                    size = reader_variant_size(0, &it);
                    if (0 == size) return 0;
                    else data_size += size;
                }
                break;
            }

            case variant::id<varlst>(): {
                BIT64 size;
                for (const auto& it : _var->to<varlst>()) {
                    size = reader_variant_size(0, &it);
                    if (0 == size) return 0;
                    else data_size += size;
                }
                break;
            }

            default: {
                if (_var->null()) data_size = 0;
                else return 0;
            }
            }
            if (data_size > _MAX_META__) return 0;
            return data_size + serable::EMPTY_SIZE + _nsize;

#undef INSTALL_CPTR_TYPE
#undef INSTALL_TYPE
#undef INSTALL_BASE_TYPE
        }
        BIT64 reader_variant_size(
            uint_64 _nsize, const variant* _var,
            std::unordered_map<const variant*, BIT64>& _dsize_map) {
#define INSTALL_BASE_TYPE(VTYPE, RTYPE) \
    case variant::id<VTYPE>(): data_size = RTYPE::sdata_size(_var->to<VTYPE>()); break;
#define INSTALL_TYPE(VTYPE, RTYPE)                          \
    INSTALL_BASE_TYPE(VTYPE, RTYPE);                        \
    INSTALL_BASE_TYPE(std::vector<VTYPE>, s_vector<VTYPE>); \
    INSTALL_BASE_TYPE(std::list<VTYPE>, s_list<VTYPE>);
#define INSTALL_CPTR_TYPE(VTYPE, RTYPE, CTYPE)                          \
    case variant::id<CTYPE<VTYPE>>(): {                                 \
        for (const VTYPE& it : _var->to<CTYPE<VTYPE>>()) {              \
            data_size += (serable::EMPTY_SIZE + RTYPE::sdata_size(it)); \
        }                                                               \
        break;                                                          \
    }

            if (_nsize > _MAX_NAME__) return 0;

            BIT64 data_size{0};
            switch (_var->type()) {
                INSTALL_TYPE(bool, s_bool);
                INSTALL_TYPE(int_8, s_int_8);
                INSTALL_TYPE(int_16, s_int_16);
                INSTALL_TYPE(int_32, s_int_32);
                INSTALL_TYPE(int_64, s_int_64);
                INSTALL_TYPE(uint_8, s_uint_8);
                INSTALL_TYPE(uint_16, s_uint_16);
                INSTALL_TYPE(uint_32, s_uint_32);
                INSTALL_TYPE(uint_64, s_uint_64);
                INSTALL_TYPE(real_32, s_real_32);
                INSTALL_TYPE(real_64, s_real_64);

                INSTALL_BASE_TYPE(bytes, s_bytes);
                INSTALL_CPTR_TYPE(bytes, s_bytes, std::vector);
                INSTALL_CPTR_TYPE(bytes, s_bytes, std::list);

                INSTALL_BASE_TYPE(std::string, s_string);
                INSTALL_CPTR_TYPE(std::string, s_string, std::vector);
                INSTALL_CPTR_TYPE(std::string, s_string, std::list);

            case variant::id<varmap>(): {
                BIT64 size;
                for (const auto& it : _var->to<varmap>()) {
                    size = reader_variant_size(it.first.size(), it.second, _dsize_map);
                    if (0 == size) return 0;
                    else data_size += size;
                }
                break;
            }

            case variant::id<varvec>(): {
                BIT64 size;
                for (const auto& it : _var->to<varvec>()) {
                    size = reader_variant_size(0, &it, _dsize_map);
                    if (0 == size) return 0;
                    else data_size += size;
                }
                break;
            }

            case variant::id<varlst>(): {
                BIT64 size;
                for (const auto& it : _var->to<varlst>()) {
                    size = reader_variant_size(0, &it, _dsize_map);
                    if (0 == size) return 0;
                    else data_size += size;
                }
                break;
            }

            default: {
                if (_var->null()) data_size = 0;
                else return 0;
            }
            }
            if (data_size > _MAX_META__) return 0;
            _dsize_map.insert({_var, data_size});
            return data_size + serable::EMPTY_SIZE + _nsize;

#undef INSTALL_CPTR_TYPE
#undef INSTALL_TYPE
#undef INSTALL_BASE_TYPE
        }
        // writes into a buffer the caller sized from reader_variant_size(): no bound is checked here
        BIT8* reader_variant_read(
            BIT8* _ptr, const char* _name, const uint_64 _nsize, const variant* _var) {
#define CHECK_PTR() \
    if (nullptr == _ptr) return nullptr
#define INSTALL_BASE_TYPE(VTYPE, RTYPE)                                                       \
    case variant::id<VTYPE>():                                                                \
        _ptr = RTYPE::swrite_head(_ptr, _name, _nsize, RTYPE::sdata_size(_var->to<VTYPE>())); \
        CHECK_PTR();                                                                          \
        _ptr = RTYPE::swrite_data(_ptr, _var->to<VTYPE>());                                   \
        CHECK_PTR();                                                                          \
        return RTYPE::swrite_tail(_ptr)
#define INSTALL_TYPE(VTYPE, RTYPE)                          \
    INSTALL_BASE_TYPE(VTYPE, RTYPE);                        \
    INSTALL_BASE_TYPE(std::vector<VTYPE>, s_vector<VTYPE>); \
    INSTALL_BASE_TYPE(std::list<VTYPE>, s_list<VTYPE>);
#define INSTALL_CPTR_TYPE(VTYPE, RTYPE, CTYPE, CRTYPE)                                                          \
    case variant::id<CTYPE<VTYPE>>(): {                                                                         \
        BIT8* h_ptr = _ptr;                                                                                     \
        _ptr += serable::DATAP_OFST + _nsize;                                                                   \
        for (const VTYPE& it : _var->to<CTYPE<VTYPE>>()) {                                                      \
            _ptr = RTYPE::swrite_head(_ptr, nullptr, 0, RTYPE::sdata_size(it));                                 \
            CHECK_PTR();                                                                                        \
            _ptr = RTYPE::swrite_data(_ptr, it);                                                                \
            CHECK_PTR();                                                                                        \
            _ptr = RTYPE::swrite_tail(_ptr);                                                                    \
            CHECK_PTR();                                                                                        \
        }                                                                                                       \
        h_ptr = CRTYPE<RTYPE*>::swrite_head(h_ptr, _name, _nsize, _ptr - h_ptr - serable::DATAP_OFST - _nsize); \
        if (nullptr == h_ptr) return nullptr;                                                                   \
        return CRTYPE<RTYPE*>::swrite_tail(_ptr);                                                               \
    }

            CHECK_PTR();

            switch (_var->type()) {
                INSTALL_TYPE(bool, s_bool);
                INSTALL_TYPE(int_8, s_int_8);
                INSTALL_TYPE(int_16, s_int_16);
                INSTALL_TYPE(int_32, s_int_32);
                INSTALL_TYPE(int_64, s_int_64);
                INSTALL_TYPE(uint_8, s_uint_8);
                INSTALL_TYPE(uint_16, s_uint_16);
                INSTALL_TYPE(uint_32, s_uint_32);
                INSTALL_TYPE(uint_64, s_uint_64);
                INSTALL_TYPE(real_32, s_real_32);
                INSTALL_TYPE(real_64, s_real_64);

                INSTALL_BASE_TYPE(bytes, s_bytes);
                INSTALL_CPTR_TYPE(bytes, s_bytes, std::vector, s_vector);
                INSTALL_CPTR_TYPE(bytes, s_bytes, std::list, s_list);

                INSTALL_BASE_TYPE(std::string, s_string);
                INSTALL_CPTR_TYPE(std::string, s_string, std::vector, s_vector);
                INSTALL_CPTR_TYPE(std::string, s_string, std::list, s_list);

            case variant::id<varmap>(): {
                BIT8* h_ptr = _ptr;
                // room for the head and the name, both written below once the payload size is known
                _ptr += serable::DATAP_OFST + _nsize;
                for (const auto& it : _var->to<varmap>()) {
                    _ptr = reader_variant_read(_ptr,
                                               it.first.data(), it.first.size(),
                                               it.second);
                    CHECK_PTR();
                }
                h_ptr = s_varmap::swrite_head(h_ptr, _name, _nsize, _ptr - h_ptr - serable::DATAP_OFST - _nsize);
                if (nullptr == h_ptr) return nullptr;
                return s_varmap::swrite_tail(_ptr);
            }

            case variant::id<varvec>(): {
                BIT8* h_ptr = _ptr;
                _ptr += serable::DATAP_OFST + _nsize;
                for (const auto& it : _var->to<varvec>()) {
                    _ptr = reader_variant_read(_ptr,
                                               nullptr, 0,
                                               &it);
                    CHECK_PTR();
                }
                h_ptr = s_varvec::swrite_head(h_ptr, _name, _nsize, _ptr - h_ptr - serable::DATAP_OFST - _nsize);
                if (nullptr == h_ptr) return nullptr;
                return s_varvec::swrite_tail(_ptr);
            }

            case variant::id<varlst>(): {
                BIT8* h_ptr = _ptr;
                _ptr += serable::DATAP_OFST + _nsize;
                for (const auto& it : _var->to<varlst>()) {
                    _ptr = reader_variant_read(_ptr,
                                               nullptr, 0,
                                               &it);
                    CHECK_PTR();
                }
                h_ptr = s_varlst::swrite_head(h_ptr, _name, _nsize, _ptr - h_ptr - serable::DATAP_OFST - _nsize);
                if (nullptr == h_ptr) return nullptr;
                return s_varlst::swrite_tail(_ptr);
            }

            default: {
                if (_var->null()) {
                    _ptr = serable::swrite_head(_ptr, _void_, _name, _nsize, 0);
                    CHECK_PTR();
                    return serable::swrite_tail(_ptr);
                }
                return nullptr;
            }
            }

#undef INSTALL_CPTR_TYPE
#undef INSTALL_TYPE
#undef INSTALL_BASE_TYPE
#undef CHECK_PTR
        }
        bool reader_variant_read(
            ostream& _ostm, const char* _name, const uint_64 _nsize, const variant* _var,
            const std::unordered_map<const variant*, BIT64>& _dsize_map) {
#define INSTALL_BASE_TYPE(VTYPE, RTYPE)                            \
    case variant::id<VTYPE>():                                     \
        return RTYPE::swrite_head(_ostm, _name, _nsize, _dsize) && \
               RTYPE::swrite_data(_ostm, _var->to<VTYPE>()) &&     \
               RTYPE::swrite_tail(_ostm)
#define INSTALL_TYPE(VTYPE, RTYPE)                          \
    INSTALL_BASE_TYPE(VTYPE, RTYPE);                        \
    INSTALL_BASE_TYPE(std::vector<VTYPE>, s_vector<VTYPE>); \
    INSTALL_BASE_TYPE(std::list<VTYPE>, s_list<VTYPE>);
#define INSTALL_CPTR_TYPE(VTYPE, RTYPE, CTYPE, CRTYPE)                                       \
    case variant::id<CTYPE<VTYPE>>(): {                                                      \
        if (!CRTYPE<RTYPE*>::swrite_head(_ostm, _name, _nsize, _dsize)) return false;        \
        for (const VTYPE& it : _var->to<CTYPE<VTYPE>>())                                     \
            if (!(RTYPE::swrite_head(_ostm, nullptr, 0, RTYPE::sdata_size(it)) &&            \
                  RTYPE::swrite_data(_ostm, it) && RTYPE::swrite_tail(_ostm))) return false; \
        return CRTYPE<RTYPE*>::swrite_tail(_ostm);                                           \
    }

            // the size pass cached this node's payload: an append-only stream cannot be patched later
            BIT64 _dsize = map_value(_dsize_map, _var);
            switch (_var->type()) {
                INSTALL_TYPE(bool, s_bool);
                INSTALL_TYPE(int_8, s_int_8);
                INSTALL_TYPE(int_16, s_int_16);
                INSTALL_TYPE(int_32, s_int_32);
                INSTALL_TYPE(int_64, s_int_64);
                INSTALL_TYPE(uint_8, s_uint_8);
                INSTALL_TYPE(uint_16, s_uint_16);
                INSTALL_TYPE(uint_32, s_uint_32);
                INSTALL_TYPE(uint_64, s_uint_64);
                INSTALL_TYPE(real_32, s_real_32);
                INSTALL_TYPE(real_64, s_real_64);

                INSTALL_BASE_TYPE(bytes, s_bytes);
                INSTALL_CPTR_TYPE(bytes, s_bytes, std::vector, s_vector);
                INSTALL_CPTR_TYPE(bytes, s_bytes, std::list, s_list);

                INSTALL_BASE_TYPE(std::string, s_string);
                INSTALL_CPTR_TYPE(std::string, s_string, std::vector, s_vector);
                INSTALL_CPTR_TYPE(std::string, s_string, std::list, s_list);

            case variant::id<varmap>(): {
                if (!s_varmap::swrite_head(_ostm, _name, _nsize, _dsize)) return false;
                for (const auto& it : _var->to<varmap>())
                    if (!reader_variant_read(_ostm,
                                             it.first.data(), it.first.size(),
                                             it.second, _dsize_map)) return false;
                return s_varmap::swrite_tail(_ostm);
            }

            case variant::id<varvec>(): {
                if (!s_varvec::swrite_head(_ostm, _name, _nsize, _dsize)) return false;
                for (const auto& it : _var->to<varvec>())
                    if (!reader_variant_read(_ostm,
                                             nullptr, 0,
                                             &it, _dsize_map)) return false;
                return s_varvec::swrite_tail(_ostm);
            }

            case variant::id<varlst>(): {
                if (!s_varlst::swrite_head(_ostm, _name, _nsize, _dsize)) return false;
                for (const auto& it : _var->to<varlst>())
                    if (!reader_variant_read(_ostm,
                                             nullptr, 0,
                                             &it, _dsize_map)) return false;
                return s_varlst::swrite_tail(_ostm);
            }

            default: {
                if (_var->null())
                    return serable::swrite_head(_ostm, _void_, _name, _nsize, 0) &&
                           serable::swrite_tail(_ostm);
                else return false;
            }
            }

#undef INSTALL_CPTR_TYPE
#undef INSTALL_TYPE
#undef INSTALL_BASE_TYPE
        }

        class deserer_var
            : public deserer<varmap, variant> {
        public:
            template <typename T>
            static void deserer_var_base_ordinary(const deserer_var::METADAT& _dat, variant& _rst) {
                // a payload shorter than T leaves the element default, rather than reading past it
                if (_dat.size < sizeof(T)) return;
                _rst = m_interpret<T>(_dat.ptr);
            }
            template <typename T>
            static void deserer_var_vec_ordinary(const deserer_var::METADAT& _dat, variant& _rst) { _rst = std::move(r_interpret<T>(_dat.ptr, _dat.size)); }
            template <typename T>
            static void deserer_var_list_ordinary(const deserer_var::METADAT& _dat, variant& _rst) {
                std::list<T>& result = _rst.as<std::list<T>>();
                const BIT8 *ptr = _dat.ptr, *end = _dat.ptr + _dat.size;
                // a partial trailing element is dropped, as r_interpret drops it for a vector
                while (ptr + sizeof(T) <= end) {
                    result.push_back(m_interpret<T>(ptr));
                    ptr += sizeof(T);
                }
            }
            static void deserer_var_base_bytes(const deserer_var::METADAT& _dat, variant& _rst) {
                bytes& result = _rst.as<bytes>();
                result.resize(_dat.size);
                memcpy(result.data(), _dat.ptr, result.size());
            }
            static void deserer_var_vec_bytes(const deserer_var::METADAT& _dat, variant& _rst) {
                std::vector<bytes>& result = _rst.as<std::vector<bytes>>();
                for_each_meta(_dat.ptr, _dat.ptr + _dat.size, [&](const METADAT& meta) {
                    bytes temp(meta.size);
                    memcpy(temp.data(), meta.ptr, temp.size());
                    result.push_back(temp);
                    return true;
                });
            }
            static void deserer_var_list_bytes(const deserer_var::METADAT& _dat, variant& _rst) {
                std::list<bytes>& result = _rst.as<std::list<bytes>>();
                for_each_meta(_dat.ptr, _dat.ptr + _dat.size, [&](const METADAT& meta) {
                    bytes temp(meta.size);
                    memcpy(temp.data(), meta.ptr, temp.size());
                    result.push_back(temp);
                    return true;
                });
            }
            static void deserer_var_base_string(const deserer_var::METADAT& _dat, variant& _rst) {
                _rst = std::string((char*) _dat.ptr, _dat.size);
            }
            static void deserer_var_vec_string(const deserer_var::METADAT& _dat, variant& _rst) {
                std::vector<std::string>& result = _rst.as<std::vector<std::string>>();
                for_each_meta(_dat.ptr, _dat.ptr + _dat.size, [&](const METADAT& meta) {
                    result.push_back(std::string((char*) meta.ptr, meta.size));
                    return true;
                });
            }
            static void deserer_var_list_string(const deserer_var::METADAT& _dat, variant& _rst) {
                std::list<std::string>& result = _rst.as<std::list<std::string>>();
                for_each_meta(_dat.ptr, _dat.ptr + _dat.size, [&](const METADAT& meta) {
                    result.push_back(std::string((char*) meta.ptr, meta.size));
                    return true;
                });
            }
            static void deserer_var_vec_any(const deserer_var::METADAT& _dat, variant& _rst) {
                varvec& result = _rst.as<varvec>();
                for_each_meta(_dat.ptr, _dat.ptr + _dat.size, [&](const METADAT& meta) {
                    // read_dat fills an element in place: the comma expression yields the new one
                    read_dat(meta, (result.push_back(variant()), result.back()));
                    return true;
                });
            }
            static void deserer_var_list_any(const deserer_var::METADAT& _dat, variant& _rst) {
                varlst& result = _rst.as<varlst>();
                for_each_meta(_dat.ptr, _dat.ptr + _dat.size, [&](const METADAT& meta) {
                    read_dat(meta, (result.push_back(variant()), result.back()));
                    return true;
                });
            }
            static bool init_core() {
                static bool is_init = []() {
                    CORE_BASE = std::unordered_map<BIT16, deserer_var::COREFUNC>{
                        {_bool_, &deserer_var::deserer_var_base_ordinary<bool>},
                        {_int8_, &deserer_var::deserer_var_base_ordinary<int_8>},
                        {_int16_, &deserer_var::deserer_var_base_ordinary<int_16>},
                        {_int32_, &deserer_var::deserer_var_base_ordinary<int_32>},
                        {_int64_, &deserer_var::deserer_var_base_ordinary<int_64>},
                        {_uint8_, &deserer_var::deserer_var_base_ordinary<uint_8>},
                        {_uint16_, &deserer_var::deserer_var_base_ordinary<uint_16>},
                        {_uint32_, &deserer_var::deserer_var_base_ordinary<uint_32>},
                        {_uint64_, &deserer_var::deserer_var_base_ordinary<uint_64>},
                        {_real32_, &deserer_var::deserer_var_base_ordinary<real_32>},
                        {_real64_, &deserer_var::deserer_var_base_ordinary<real_64>},
                        {_bytes_, &deserer_var::deserer_var_base_bytes},
                        {_string_, &deserer_var::deserer_var_base_string},
                    };
                    CORE__VEC = std::unordered_map<BIT16, deserer_var::COREFUNC>{
                        {_bool_, &deserer_var::deserer_var_vec_ordinary<bool>},
                        {_int8_, &deserer_var::deserer_var_vec_ordinary<int_8>},
                        {_int16_, &deserer_var::deserer_var_vec_ordinary<int_16>},
                        {_int32_, &deserer_var::deserer_var_vec_ordinary<int_32>},
                        {_int64_, &deserer_var::deserer_var_vec_ordinary<int_64>},
                        {_uint8_, &deserer_var::deserer_var_vec_ordinary<uint_8>},
                        {_uint16_, &deserer_var::deserer_var_vec_ordinary<uint_16>},
                        {_uint32_, &deserer_var::deserer_var_vec_ordinary<uint_32>},
                        {_uint64_, &deserer_var::deserer_var_vec_ordinary<uint_64>},
                        {_real32_, &deserer_var::deserer_var_vec_ordinary<real_32>},
                        {_real64_, &deserer_var::deserer_var_vec_ordinary<real_64>},
                        {_bytes_, &deserer_var::deserer_var_vec_bytes},
                        {_string_, &deserer_var::deserer_var_vec_string},
                        {_any_, &deserer_var::deserer_var_vec_any},
                    };
                    CORE_LIST = std::unordered_map<BIT16, deserer_var::COREFUNC>{
                        {_bool_, &deserer_var::deserer_var_list_ordinary<bool>},
                        {_int8_, &deserer_var::deserer_var_list_ordinary<int_8>},
                        {_int16_, &deserer_var::deserer_var_list_ordinary<int_16>},
                        {_int32_, &deserer_var::deserer_var_list_ordinary<int_32>},
                        {_int64_, &deserer_var::deserer_var_list_ordinary<int_64>},
                        {_uint8_, &deserer_var::deserer_var_list_ordinary<uint_8>},
                        {_uint16_, &deserer_var::deserer_var_list_ordinary<uint_16>},
                        {_uint32_, &deserer_var::deserer_var_list_ordinary<uint_32>},
                        {_uint64_, &deserer_var::deserer_var_list_ordinary<uint_64>},
                        {_real32_, &deserer_var::deserer_var_list_ordinary<real_32>},
                        {_real64_, &deserer_var::deserer_var_list_ordinary<real_64>},
                        {_bytes_, &deserer_var::deserer_var_list_bytes},
                        {_string_, &deserer_var::deserer_var_list_string},
                        {_any_, &deserer_var::deserer_var_list_any},
                    };
                    return true;
                }();
                return is_init;
            }
        };
    }
}

using namespace alx::ser;

bool alx::varsolid::to_bytes(const varmap& _vmap, bytes& _buff, variant** _err_obj) {
#define RETURN_FALSE() return nullptr == _err_obj ? false : (*_err_obj = it.second, false)

    BIT64 dsize{0}, tsize;
    for (const auto& it : _vmap) {
        tsize = reader_variant_size(it.first.size(), it.second);
        if (0 == tsize) RETURN_FALSE();
        else dsize += tsize;
    }

    if (dsize > _MAX_DATA__) return false;
    _buff.resize(dsize + serer::EMPTY_SIZE);
    BIT8* ptr = _buff.data();
    ptr = serer::swrite_head(ptr, dsize);
    if (nullptr == ptr) return false;

    for (const auto& it : _vmap) {
        ptr = reader_variant_read(ptr, it.first.data(), it.first.size(), it.second);
        if (nullptr == ptr) RETURN_FALSE();
    }

    ptr = serer::swrite_tail(ptr);
    if (nullptr == ptr || ptr != _buff.end()) return false;

    return true;

#undef RETURN_FALSE
}

bool alx::varsolid::to_bytes(const varmap& _vmap, ostream& _ostm, variant** _err_obj) {
#define RETURN_FALSE() return nullptr == _err_obj ? false : (*_err_obj = it.second, false)

    BIT64 dsize{0}, tsize;
    const uint_64 bsize{_ostm.total()};
    std::unordered_map<const variant*, BIT64> dsize_map;
    for (const auto& it : _vmap) {
        tsize = reader_variant_size(it.first.size(), it.second, dsize_map);
        if (0 == tsize) RETURN_FALSE();
        else dsize += tsize;
    }

    if (!serer::swrite_head(_ostm, dsize)) return false;

    for (const auto& it : _vmap)
        if (!reader_variant_read(_ostm, it.first.data(), it.first.size(), it.second, dsize_map)) RETURN_FALSE();

    if (!serer::swrite_tail(_ostm) || (_ostm.total() - bsize) != (dsize + serer::EMPTY_SIZE)) return false;

    return true;

#undef RETURN_FALSE
}

varmap alx::varsolid::to_varmap(const bytes_view& _bytes) {
    deserer_var::init_core();
    return deserer_var::parse(_bytes);
}

bool alx::varsolid::to_varmap(const bytes_view& _bytes, varmap& _vmap) {
    deserer_var::init_core();
    return deserer_var::valid(_bytes) ? _vmap = deserer_var::parse(_bytes), true : false;
}

bool alx::varsolid::is_valid(const bytes_view& _bytes) {
    return deserer_var::valid(_bytes);
}

variant alx::varsolid::get_value(const bytes_view& _bytes, const std::list<std::string>& _path, const variant& _def) {
    deserer_var::init_core();
    return deserer_var::parse(_bytes, _path, _def);
}
