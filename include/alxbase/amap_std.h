/*****************************************************************/ /**
 * \file   amap_std.h
 * \brief  conver: build one container from another container's entries
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#ifndef _ALEXIS_MAP_STD_H_
#define _ALEXIS_MAP_STD_H_

#include "amap.h"

#include <type_traits>
#include <unordered_map>
#include <utility>

namespace alx {

    namespace amap_std_detail {
        /// Reserve room in the targets that have a reserve; the ordered ones have none
        template <typename To>
        void reserve_for(To&, size_t) {}
        template <typename K, typename V, typename H, typename E>
        void reserve_for(std::unordered_map<K, V, H, E>& _m, size_t _n) { _m.reserve(_n); }
        template <typename K, typename V, bool O, typename H>
        void reserve_for(map_hash<K, V, O, H>& _m, size_t _n) { _m.reserve(_n); }
        template <typename K, typename V, bool O, typename H>
        void reserve_for(map_read<K, V, O, H>& _m, size_t _n) { _m.reserve(_n); }

        /// An owning map hands over a raw pointee through emplace() without cloning, so a conversion cannot take one on either side
        template <typename T>
        struct owns_pointees : std::false_type {};
        template <typename K, typename V>
        struct owns_pointees<map_btree<K, V, true>> : std::true_type {};
        template <typename K, typename V, typename H>
        struct owns_pointees<map_hash<K, V, true, H>> : std::true_type {};
        template <typename K, typename V, typename H>
        struct owns_pointees<map_read<K, V, true, H>> : std::true_type {};

        template <typename To, typename From>
        struct conver_plain : std::integral_constant<bool, !owns_pointees<To>::value && !owns_pointees<typename std::remove_cv<typename std::remove_reference<From>::type>::type>::value> {};
    }

    /// Copy every entry of _src into a fresh To
    template <typename To, typename From>
    To conver(const From& _src) {
        static_assert(amap_std_detail::conver_plain<To, From>::value, "conver: an owning map is not convertible -- detach or consume it first");
        To out;
        amap_std_detail::reserve_for(out, _src.size());
        for (auto it = _src.begin(); it != _src.end(); ++it) out.emplace(it->first, it->second);
        return out;
    }

    /// Move every entry of _src into a fresh To and empty the source
    template <typename To, typename From>
    typename std::enable_if<!std::is_lvalue_reference<From>::value && !std::is_const<typename std::remove_reference<From>::type>::value, To>::type
    conver(From&& _src) {
        static_assert(amap_std_detail::conver_plain<To, From>::value, "conver: an owning map is not convertible -- detach or consume it first");
        To out;
        amap_std_detail::reserve_for(out, _src.size());
        for (auto it = _src.begin(); it != _src.end(); ++it) out.emplace(std::move(it->first), std::move(it->second));
        _src.clear();
        return out;
    }
}

#endif
