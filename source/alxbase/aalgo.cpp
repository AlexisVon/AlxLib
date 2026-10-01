/*****************************************************************/ /**
 * \file   aalgo.cpp
 * \brief  General algorithm library (sorting, searching, etc.)
 * 
 * \author alexis
 * 
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "aalgo.h"
#include <climits>
#include <cstring>

using namespace alx;

uint_64 alx::find(
    const void* _dst, const uint_64 _dsz,
    const void* _val, const uint_64 _vsz,
    const uint_64 _from, const uint_64 _to) {
    if (0 == _dsz || nullptr == _val) return uint_64_npos;
    const uint_64 _beg = _from;
    const uint_64 _end = alx::min_value(_dsz, _to);
    const uint_64 _len = _end - _beg;
    if (_beg >= _end || _len < _vsz) return uint_64_npos;

    // a zero-length pattern still needs a non-empty window: _beg < _end
    if (_vsz == 0) return _beg;
    else if (_vsz == 1) return find_func::find_by_val<uint_8>(_dst, _val, _beg, _end);
    else if (_vsz == 2) return find_func::find_by_val<uint_16>(_dst, _val, _beg, _end);
    else if (_vsz == 4) return find_func::find_by_val<uint_32>(_dst, _val, _beg, _end);
    else if (_vsz == 8) return find_func::find_by_val<uint_64>(_dst, _val, _beg, _end);
    else if (_vsz <= 8 && _len < 512) return find_func::find_by_brute_force(_dst, _val, _vsz, _beg, _end);
    else if (_vsz <= 16 && _len < 512) return find_func::find_by_rabin_karp(_dst, _val, _vsz, _beg, _end);
    // not only long patterns: a short one in a long window lands here too
    else return find_func::find_by_boyer_moore(_dst, _val, _vsz, _beg, _end);
}

uint_64 alx::rfind(
    const void* _dst, const uint_64 _dsz,
    const void* _val, const uint_64 _vsz,
    const uint_64 _rfrom, const uint_64 _rto) {
    if (0 == _dsz || nullptr == _val) return uint_64_npos;
    const uint_64 _rbeg = alx::min_value(_dsz - 1, _rfrom);
    const uint_64 _rend = _rto;
    const uint_64 _len = _rbeg - _rend + 1;
    if (_rbeg < _rend || _len < _vsz) return uint_64_npos;

    // a zero-length pattern still needs a non-empty window: _rbeg >= _rend
    if (_vsz == 0) return _rbeg;
    else if (_vsz == 1) return find_func::rfind_by_val<uint_8>(_dst, _val, _rbeg, _rend);
    else if (_vsz == 2) return find_func::rfind_by_val<uint_16>(_dst, _val, _rbeg, _rend);
    else if (_vsz == 4) return find_func::rfind_by_val<uint_32>(_dst, _val, _rbeg, _rend);
    else if (_vsz == 8) return find_func::rfind_by_val<uint_64>(_dst, _val, _rbeg, _rend);
    else if (_vsz <= 8 && _len < 512) return find_func::rfind_by_brute_force(_dst, _val, _vsz, _rbeg, _rend);
    else if (_vsz <= 16 && _len < 512) return find_func::rfind_by_rabin_karp(_dst, _val, _vsz, _rbeg, _rend);
    else return find_func::rfind_by_boyer_moore(_dst, _val, _vsz, _rbeg, _rend);
}

std::pair<uint_64, uint_64> alx::find_enum(
    const void* _dst, const uint_64 _dsz,
    const void** _val_lst, const uint_64 _val_lsz, const uint_64 _vsz,
    const uint_64 _from, const uint_64 _to) {
    if (0 == _dsz || nullptr == _val_lst || 0 == _val_lsz) return {uint_64_npos, uint_64_npos};
    const uint_64 _beg = _from;
    const uint_64 _end = alx::min_value(_dsz, _to);
    if (_beg >= _end || _end - _beg < _vsz) return {uint_64_npos, uint_64_npos};

    if (_vsz == 0) return {_beg, 0};
    else if (_vsz == 1) return find_func::find_enum_by_val<uint_8>(_dst, _val_lst, _val_lsz, _beg, _end);
    else if (_vsz == 2) return find_func::find_enum_by_val<uint_16>(_dst, _val_lst, _val_lsz, _beg, _end);
    else if (_vsz == 4) return find_func::find_enum_by_val<uint_32>(_dst, _val_lst, _val_lsz, _beg, _end);
    else if (_vsz == 8) return find_func::find_enum_by_val<uint_64>(_dst, _val_lst, _val_lsz, _beg, _end);

    const uint_64 _minus_1 = _vsz - 1;
    const uint_8* _dst_t = (const uint_8*) _dst;
    const uint_8** _val_lst_t = (const uint_8**) _val_lst;
    const uint_8* _crt_t = _dst_t + _beg;
    const uint_8* _end_t = _dst_t + _end - _vsz;

    uint_64 hash_dst{0};
    uint_64* hash_val_lst = new uint_64[_val_lsz]{0};
    for (uint_64 i = 0; i < _vsz; ++i) {
        hash_dst = ((hash_dst << 1) + _crt_t[i]);
        for (uint_64 j = 0; j < _val_lsz; ++j) hash_val_lst[j] = ((hash_val_lst[j] << 1) + _val_lst_t[j][i]);
    }
    // the window's last byte is dropped here, each turn adds it back before the compare
    hash_dst -= _crt_t[_minus_1];

    std::unordered_map<uint_64, std::vector<uint_64>> hash_val_map;
    for (uint_64 i = 0; i < _val_lsz; i++) hash_val_map[hash_val_lst[i]].push_back(i);
    delete[] hash_val_lst;

    while (_crt_t <= _end_t) {
        hash_dst += _crt_t[_minus_1];
        auto iter = hash_val_map.find(hash_dst);
        if (iter != hash_val_map.end())
            for (uint_64 index : iter->second)
                if (memcmp(_val_lst_t[index], _crt_t, _vsz) == 0) return {_crt_t - _dst_t, index};
        if (_minus_1 < sizeof(uint_64) * CHAR_BIT) hash_dst -= uint_64(_crt_t[0]) << _minus_1;
        hash_dst <<= 1;
        ++_crt_t;
    }
    return {uint_64_npos, uint_64_npos};
}

std::pair<uint_64, uint_64> alx::rfind_enum(
    const void* _dst, const uint_64 _dsz,
    const void** _val_lst, const uint_64 _val_lsz, const uint_64 _vsz,
    const uint_64 _rfrom, const uint_64 _rto) {
    if (0 == _dsz || nullptr == _val_lst || 0 == _val_lsz) return {uint_64_npos, uint_64_npos};
    const uint_64 _rbeg = alx::min_value(_dsz - 1, _rfrom);
    const uint_64 _rend = _rto;
    if (_rbeg < _rend || _rbeg - _rend + 1 < _vsz) return {uint_64_npos, uint_64_npos};

    if (_vsz == 0) return {_rbeg, 0};
    else if (_vsz == 1) return find_func::rfind_enum_by_val<uint_8>(_dst, _val_lst, _val_lsz, _rbeg, _rend);
    else if (_vsz == 2) return find_func::rfind_enum_by_val<uint_16>(_dst, _val_lst, _val_lsz, _rbeg, _rend);
    else if (_vsz == 4) return find_func::rfind_enum_by_val<uint_32>(_dst, _val_lst, _val_lsz, _rbeg, _rend);
    else if (_vsz == 8) return find_func::rfind_enum_by_val<uint_64>(_dst, _val_lst, _val_lsz, _rbeg, _rend);

    const uint_64 _minus_1 = _vsz - 1;
    const uint_8* _dst_t = (const uint_8*) _dst;
    const uint_8** _val_lst_t = (const uint_8**) _val_lst;
    const uint_8* _crt_t = _dst_t + _rbeg - _minus_1;
    const uint_8* _end_t = _dst_t + _rend;

    uint_64 hash_dst{0};
    uint_64* hash_val_lst = new uint_64[_val_lsz]{0};
    for (uint_64 i = _vsz; i-- > 0;) {
        hash_dst = ((hash_dst << 1) + _crt_t[i]);
        for (uint_64 j = 0; j < _val_lsz; ++j) hash_val_lst[j] = ((hash_val_lst[j] << 1) + _val_lst_t[j][i]);
    }
    // the window's first byte is dropped here, each turn adds it back before the compare
    hash_dst -= _crt_t[0];

    std::unordered_map<uint_64, std::vector<uint_64>> hash_val_map;
    for (uint_64 i = 0; i < _val_lsz; i++) hash_val_map[hash_val_lst[i]].push_back(i);
    delete[] hash_val_lst;

    while (_crt_t >= _end_t) {
        hash_dst += _crt_t[0];
        auto iter = hash_val_map.find(hash_dst);
        if (iter != hash_val_map.end())
            for (uint_64 index : iter->second)
                if (memcmp(_val_lst_t[index], _crt_t, _vsz) == 0) return {_crt_t - _dst_t, index};
        if (_minus_1 < sizeof(uint_64) * CHAR_BIT) hash_dst -= uint_64(_crt_t[_minus_1]) << _minus_1;
        hash_dst <<= 1;
        --_crt_t;
    }
    return {uint_64_npos, uint_64_npos};
}

uint_64 alx::find_func::find_by_kmp(const void* _dst, const void* _val, uint_64 _len, uint_64 _beg, uint_64 _end) {
    const uint_8* _dst_t = (const uint_8*) _dst;
    const uint_8* _val_t = (const uint_8*) _val;
    const uint_8* _crt_t = _dst_t + _beg;
    const uint_8* _end_t = _dst_t + _end;
    const std::vector<uint_64> lps = compute_lps(_val_t, _len);
    const uint_8* _val_beg = _val_t;
    const uint_8* _val_end = _val_t + _len;
    _val_t = _val_beg;
    while (_crt_t < _end_t && _val_t < _val_end) {
        if (*_val_t == *_crt_t) {
            _crt_t++;
            _val_t++;
        } else {
            if (_val_t == _val_beg) _crt_t++;
            else _val_t = _val_beg + lps[_val_t - _val_beg - 1];
        }
    }
    return _val_t == _val_end ? _crt_t - _dst_t - _len : uint_64_npos;
}

uint_64 alx::find_func::find_by_rabin_karp(const void* _dst, const void* _val, uint_64 _len, uint_64 _beg, uint_64 _end) {
    const uint_64 _minus_1 = _len - 1;
    const uint_8* _dst_t = (const uint_8*) _dst;
    const uint_8* _val_t = (const uint_8*) _val;
    const uint_8* _crt_t = _dst_t + _beg;
    const uint_8* _end_t = _dst_t + _end - _len;
    uint_64 hash_dst{0}, hash_val{0};
    for (uint_64 i = 0; i < _len; ++i) {
        hash_val = ((hash_val << 1) + _val_t[i]);
        hash_dst = ((hash_dst << 1) + _crt_t[i]);
    }
    // the window's last byte is dropped here, each turn adds it back before the compare
    hash_dst -= _crt_t[_minus_1];

    while (_crt_t <= _end_t) {
        hash_dst += _crt_t[_minus_1];
        if (hash_dst == hash_val && memcmp(_val_t, _crt_t, _len) == 0) return _crt_t - _dst_t;
        if (_minus_1 < sizeof(uint_64) * CHAR_BIT) hash_dst -= uint_64(_crt_t[0]) << _minus_1;
        hash_dst <<= 1;
        ++_crt_t;
    }
    return uint_64_npos;
}

uint_64 alx::find_func::find_by_boyer_moore(const void* _dst, const void* _val, uint_64 _len, uint_64 _beg, uint_64 _end) {
    uint_8 _sk_table[256];
    {
        // per byte, its distance from the pattern's end; absent bytes hold the uint_8 fill min(_len, 255)
        uint_8 _min_l = (uint_8) alx::min_value(_len, 255ULL);
        const uint_8* _val_t = (const uint_8*) _val + _len - _min_l;
        memset(_sk_table, _min_l, 256);
        while (_min_l--) _sk_table[*_val_t++] = _min_l;
    }
    const uint_64 _minus_1 = _len - 1;
    const uint_8* _dst_t = (const uint_8*) _dst;
    const uint_8* _crt_t = _dst_t + _beg + _minus_1;
    const uint_8* _end_t = _dst_t + _end;
    const uint_8* _val_t = (const uint_8*) _val;
    while (_crt_t < _end_t) {
        uint_64 skip = _sk_table[*_crt_t];
        if (!skip) {
            while (skip < _len) {
                if (*(_crt_t - skip) != _val_t[_minus_1 - skip]) break;
                skip++;
            }
            if (skip > _minus_1) return _crt_t - _dst_t - _minus_1;
            // the fill value marks a byte the pattern lacks: the pattern passes it whole, else steps one
            skip = _sk_table[*(_crt_t - skip)] == _len ? _len - skip : 1;
        }
        _crt_t += skip;
    }
    return uint_64_npos;
}

uint_64 alx::find_func::find_by_brute_force(const void* _dst, const void* _val, uint_64 _len, uint_64 _beg, uint_64 _end) {
    const uint_8* _dst_t = (const uint_8*) _dst;
    const uint_8* _val_t = (const uint_8*) _val;
    const uint_8* _crt_t = _dst_t + _beg;
    const uint_8* _end_t = _dst_t + _end - _len;
    while (_crt_t <= _end_t)
        if (memcmp(_val_t, _crt_t, _len) == 0) return _crt_t - _dst_t;
        else _crt_t++;
    return uint_64_npos;
}

uint_64 alx::find_func::rfind_by_kmp(const void* _dst, const void* _val, uint_64 _len, uint_64 _rbeg, uint_64 _rend) {
    const uint_64 _minus_1 = _len - 1;
    const uint_8* _dst_t = (const uint_8*) _dst;
    const uint_8* _val_t = (const uint_8*) _val;
    const uint_8* _crt_t = _dst_t + _rbeg;
    const uint_8* _end_t = _dst_t + _rend;
    const std::vector<uint_64> rlps = compute_rlps(_val_t, _len);
    const uint_8* _val_beg = _val_t + _minus_1;
    const uint_8* _val_end = _val_t;
    _val_t = _val_beg;
    while (_crt_t >= _end_t && _val_t >= _val_end) {
        if (*_val_t == *_crt_t) {
            _crt_t--;
            _val_t--;
        } else {
            if (_val_t == _val_beg) _crt_t--;
            else _val_t = _val_beg - rlps[_val_beg - _val_t - 1];
        }
    }
    // a full match stops one byte below the pattern's first byte: hence the _len test and the +1
    return (uint_64) (_val_beg - _val_t) == _len ? _crt_t - _dst_t + 1 : uint_64_npos;
}

uint_64 alx::find_func::rfind_by_rabin_karp(const void* _dst, const void* _val, uint_64 _len, uint_64 _rbeg, uint_64 _rend) {
    const uint_64 _minus_1 = _len - 1;
    const uint_8* _dst_t = (const uint_8*) _dst;
    const uint_8* _val_t = (const uint_8*) _val;
    const uint_8* _crt_t = _dst_t + _rbeg - _minus_1;
    const uint_8* _end_t = _dst_t + _rend;
    uint_64 hash_dst{0}, hash_val{0};
    for (uint_64 i = _len; i-- > 0;) {
        hash_val = ((hash_val << 1) + _val_t[i]);
        hash_dst = ((hash_dst << 1) + _crt_t[i]);
    }
    // the window's first byte is dropped here, each turn adds it back before the compare
    hash_dst -= _crt_t[0];

    while (_crt_t >= _end_t) {
        hash_dst += _crt_t[0];
        if (hash_dst == hash_val && memcmp(_val_t, _crt_t, _len) == 0) return _crt_t - _dst_t;
        if (_minus_1 < sizeof(uint_64) * CHAR_BIT) hash_dst -= uint_64(_crt_t[_minus_1]) << _minus_1;
        hash_dst <<= 1;
        --_crt_t;
    }
    return uint_64_npos;
}

uint_64 alx::find_func::rfind_by_boyer_moore(const void* _dst, const void* _val, uint_64 _len, uint_64 _rbeg, uint_64 _rend) {
    uint_8 _sk_table[256];
    {
        // per byte, its offset from the pattern's start; absent bytes hold the uint_8 fill min(_len, 255)
        uint_8 _min_l = (uint_8) alx::min_value(_len, 255ULL);
        const uint_8* _val_t = (const uint_8*) _val + _min_l - 1;
        memset(_sk_table, _min_l, 256);
        while (_min_l--) _sk_table[*_val_t--] = _min_l;
    }
    const uint_64 _minus_1 = _len - 1;
    const uint_8* _dst_t = (const uint_8*) _dst;
    const uint_8* _val_t = (const uint_8*) _val;
    const uint_8* _crt_t = _dst_t + _rbeg - _minus_1;
    const uint_8* _end_t = _dst_t + _rend;
    while (_crt_t >= _end_t) {
        uint_64 skip = _sk_table[*_crt_t];
        if (!skip) {
            while (skip < _len) {
                if (_crt_t[skip] != _val_t[skip]) break;
                skip++;
            }
            if (skip > _minus_1) return _crt_t - _dst_t;
            // as above: the fill value marks a byte the pattern lacks, so it passes it whole, else steps one
            skip = _sk_table[_crt_t[skip]] == _len ? _len - skip : 1;
        }
        _crt_t -= skip;
    }
    return uint_64_npos;
}

uint_64 alx::find_func::rfind_by_brute_force(const void* _dst, const void* _val, uint_64 _len, uint_64 _rbeg, uint_64 _rend) {
    const uint_64 _minus_1 = _len - 1;
    const uint_8* _dst_t = (const uint_8*) _dst;
    const uint_8* _val_t = (const uint_8*) _val;
    const uint_8* _crt_t = _dst_t + _rbeg - _minus_1;
    const uint_8* _end_t = _dst_t + _rend;
    while (_crt_t >= _end_t)
        if (memcmp(_val_t, _crt_t, _len) == 0) return _crt_t - _dst_t;
        else _crt_t--;
    return uint_64_npos;
}
