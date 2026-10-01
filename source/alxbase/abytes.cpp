/*****************************************************************/ /**
 * \file   abytes.cpp
 * \brief  Byte array and binary data processing (COW+VIEW)
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "abytes.h"

#include <new>
#include <stdexcept>

using namespace alx;

namespace {

    // the align-up step and the block header allocated on top of it must both fit in uint_64
    inline bool block_fits(uint_64 _size, uint_8 _align) {
        if (_align >= 64) return false;
        const uint_64 step = ((uint_64) 1 << _align) - 1;
        if (_size > max_uint_64 - step) return false;
        return alx::bit_align(_size, _align) <= max_uint_64 - sizeof(bytes_block);
    }
}

bytes_block* alx::bytes_block::allocate(uint_64 _size, uint_8 _align) {
    if (!block_fits(_size, _align)) throw std::length_error("bytes: size is not representable");
    uint_64 alloc_size = alx::bit_align(_size, _align);
    bytes_block* block = reinterpret_cast<bytes_block*>(malloc(alloc_size + sizeof(bytes_block)));
    if (nullptr == block) throw std::bad_alloc();
    block->alloc = alloc_size;
    block->size = 0;
    block->ref.init_owned();
    return block;
}

bytes_block* alx::bytes_block::reallocate(bytes_block* _block, uint_64 _size, uint_8 _align) {
    if (nullptr == _block) return allocate(_size, _align);
    if (!block_fits(_size, _align)) throw std::length_error("bytes: size is not representable");

    uint_64 alloc_size = alx::bit_align(_size, _align);
    if (_block->alloc == alloc_size) return _block;

    bytes_block* block = reinterpret_cast<bytes_block*>(realloc(_block, alloc_size + sizeof(bytes_block)));
    // the old block survives a failed realloc, so the caller keeps its data
    if (nullptr == block) throw std::bad_alloc();
    block->alloc = alloc_size;
    block->ref.init_owned();
    return block;
}

alx::bytes::bytes(const bytes& _other) noexcept
    : m_data(_other.m_data) {
    if (!null()) m_data->ref.ref();
}

alx::bytes::bytes(bytes&& _other) noexcept
    : m_data(_other.m_data) {
    _other.m_data = nullptr;
}

bytes& alx::bytes::operator=(const bytes& _other) noexcept {
    if (this == &_other) return *this;
    this->~bytes();
    m_data = _other.m_data;
    if (!null()) m_data->ref.ref();
    return *this;
}

bytes& alx::bytes::operator=(bytes&& _other) noexcept {
    std::swap(m_data, _other.m_data);
    return *this;
}

alx::bytes::bytes(const void* _src, uint_64 _size)
    : m_data(bytes_block::allocate(_size)) {
    m_data->size = _size;
    memcpy(m_data->data(), _src, _size);
}

alx::bytes::bytes(uint_64 _size)
    : m_data(bytes_block::allocate(_size)) {
    m_data->size = _size;
}

alx::bytes::bytes(uint_64 _size, uint_8 _value)
    : m_data(bytes_block::allocate(_size)) {
    m_data->size = _size;
    memset(m_data->data(), _value, m_data->size);
}

void alx::bytes::revers(void* _ptr, uint_64 _len) {
    if (nullptr == _ptr || 0 == _len) return;
    if (_len <= 24) {
        uint_64 beg{0}, end{_len - 1};
        uint_8 temp, *ptr{(uint_8*) _ptr};
        while (beg < end) {
            temp = ptr[beg];
            ptr[beg] = ptr[end];
            ptr[end] = temp;
            beg++;
            end--;
        }
    } else {
        // each pass moves 16 bytes off both ends, so the middle leftover is _len mod 16
        uint_64 last = _len & 0X0FU;
        uint_64 beg{0}, end{_len - 8}, temp64;
        uint_8 temp8, *ptr8{(uint_8*) _ptr};
        while (beg + 8 <= end) {
            temp64 = byte_reverse(m_interpret<uint_64>(ptr8 + beg));
            m_interpret<uint_64>(ptr8 + beg, byte_reverse(m_interpret<uint_64>(ptr8 + end)));
            m_interpret<uint_64>(ptr8 + end, temp64);
            beg += 8;
            end -= 8;
        }
        if (last > 1) {
            end = beg + last - 1;
            while (beg < end) {
                temp8 = ptr8[beg];
                ptr8[beg] = ptr8[end];
                ptr8[end] = temp8;
                beg++;
                end--;
            }
        }
    }
}

std::string alx::bytes::to_hex(const uint_8* _ptr, const uint_64 _size) {
    static const char hex_chars[]{"0123456789abcdef"};
    if (_size == 0) return std::string();

    std::string result;
    result.resize(_size << 1);
    for (uint_32 i = 0; i < _size; i++) {
        result[((uint_64) i << 1)] = hex_chars[(_ptr[i] >> 4) & 0X0F];
        result[((uint_64) i << 1) + 1] = hex_chars[(_ptr[i]) & 0X0F];
    }
    return result;
}

std::string alx::bytes::to_base64(const uint_8* _ptr, const uint_64 _size) {
    static const char base64_chars[]{"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"};
    if (_size == 0) return std::string();

    std::string result;
    result.resize(((_size + 2) / 3) << 2);
    uint_64 i{0}, j{0};
    for (; i + 2 < _size; i += 3, j += 4) {
        uint_32 triple = (uint_32(_ptr[i]) << 16) | (uint_32(_ptr[i + 1] << 8)) | (uint_32(_ptr[i + 2]));
        result[j] = base64_chars[(triple >> 18) & 0X3F];
        result[j + 1] = base64_chars[(triple >> 12) & 0X3F];
        result[j + 2] = base64_chars[(triple >> 6) & 0X3F];
        result[j + 3] = base64_chars[(triple) & 0X3F];
    }

    if (i < _size) {
        uint_32 triple = (uint_32(_ptr[i]) << 16) | (i + 1 < _size ? uint_32(_ptr[i + 1]) << 8 : 0);

        result[j] = base64_chars[(triple >> 18) & 0X3F];
        result[j + 1] = base64_chars[(triple >> 12) & 0X3F];
        result[j + 2] = i + 1 < _size ? base64_chars[(triple >> 6) & 0X3F] : '=';
        result[j + 3] = '=';
    }

    return result;
}

std::string alx::bytes::to_string(const uint_8* _ptr, const uint_64 _size) {
    return _size == 0 ? std::string() : std::string((char*) _ptr, _size);
}

std::vector<uint_8> alx::bytes::to_vector(const uint_8* _ptr, const uint_64 _size) {
    if (_size == 0) return std::vector<uint_8>();
    std::vector<uint_8> result(_size);
    memcpy(result.data(), _ptr, _size);
    return result;
}

bytes alx::bytes::from_hex(const std::string& _v) {
    if (_v.empty() || _v.length() & 0X01LLU) return bytes();
    bytes result(_v.length() >> 1);
    const char* hex_chars = _v.c_str();
    uint_8* ptr = result.data();

    for (uint_64 i = 0; i < result.size(); i++) {
        char h_ch = hex_chars[i << 1], l_ch = hex_chars[(i << 1) + 1];
        if (h_ch >= '0' && h_ch <= '9') ptr[i] = uint_8(h_ch - '0') << 4;
        else if (h_ch >= 'A' && h_ch <= 'F') ptr[i] = uint_8(h_ch - 'A' + 10) << 4;
        else if (h_ch >= 'a' && h_ch <= 'f') ptr[i] = uint_8(h_ch - 'a' + 10) << 4;
        else return bytes();

        if (l_ch >= '0' && l_ch <= '9') ptr[i] |= uint_8(l_ch - '0');
        else if (l_ch >= 'A' && l_ch <= 'F') ptr[i] |= uint_8(l_ch - 'A' + 10);
        else if (l_ch >= 'a' && l_ch <= 'f') ptr[i] |= uint_8(l_ch - 'a' + 10);
        else return bytes();
    }

    return result;
}

bytes alx::bytes::from_base64(const std::string& _v) {
    static const uint_8 base64_table[]{
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 62, 0, 0, 0, 63,
        52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 0, 0, 0, 0, 0, 0,
        0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 0, 0, 0, 0, 0,
        0, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
        41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    if (_v.empty() || (_v.length() & 0X03LLU)) return bytes();

    uint_64 length = _v.length();
    uint_64 padding{'=' == _v[length - 1] ? '=' == _v[length - 2] ? 0X2LLU : 0X1LLU : 0X0LLU};

    bytes result((_v.length() >> 2) * 3 - padding);
    uint_8* ptr = result.data();
    uint_64 i{0}, j{0};
    while (j < result.size()) {
        uint_32 triple =
            (base64_table[uint_8(_v[i])] << 18) |
            (base64_table[uint_8(_v[i + 1])] << 12) |
            (base64_table[uint_8(_v[i + 2])] << 6) |
            (base64_table[uint_8(_v[i + 3])]);
        i += 4;
        // the padded tail group writes fewer than 3 bytes, so each store is bounded
        if (j < result.size()) ptr[j++] = (triple >> 16) & 0XFFU;
        if (j < result.size()) ptr[j++] = (triple >> 8) & 0XFFU;
        if (j < result.size()) ptr[j++] = (triple) & 0XFFU;
    }

    return result;
}

void alx::bytes::reallocate(uint_64 _size) {
    // 0 releases the payload; a block that already holds none keeps its capacity
    if (_size == 0) return null() ? void() : clear();
    if (null()) m_data = bytes_block::allocate(_size);
    else if (m_data->ref.is_shared()) {
        bytes_block* x = bytes_block::allocate(_size);
        x->size = std::min(_size, m_data->size);
        memcpy(x->data(), m_data->data(), x->size);
        if (!m_data->ref.deref()) bytes_block::deallocate(m_data);
        m_data = x;
    } else {
        uint_64 size = std::min(_size, m_data->size);
        m_data = bytes_block::reallocate(m_data, _size);
        m_data->size = size;
    }
}

void alx::bytes::ensure_capacity(uint_64 _need) {
    if (null()) m_data = bytes_block::allocate(_need);
    else if (m_data->ref.is_shared()) {
        bytes_block* x = bytes_block::allocate(_need);
        x->size = std::min(_need, m_data->size);
        memcpy(x->data(), m_data->data(), x->size);
        if (!m_data->ref.deref()) bytes_block::deallocate(m_data);
        m_data = x;
    } else if (_need > m_data->alloc) {
        const uint_64 old_size = m_data->size;
        // 1.5x growth from a 16-byte floor; a 2x step would leave freed blocks too big to be reused
        uint_64 new_size{0 == m_data->alloc ? alx::bit_align_v<1, bytes_block::default_align> : m_data->alloc};
        while (_need > new_size) {
            const uint_64 grow = new_size + (new_size >> 1);
            if (grow <= new_size) throw std::length_error("bytes: capacity is not representable");
            new_size = grow;
        }
        m_data = bytes_block::reallocate(m_data, new_size);
        m_data->size = old_size;
    }
}

bytes& alx::bytes::append(const void* _src, const uint_64 _len) {
    if (_len != 0) {
        if (size() + _len < size()) throw std::length_error("bytes: append length is not representable");
        // _src must not point into this buffer: ensure_capacity may detach and move the payload
        ensure_capacity(size() + _len);
        memcpy(m_data->data(m_data->size), _src, _len);
        m_data->size += _len;
    }
    return *this;
}

bytes alx::bytes::mid(uint_64 _pos, uint_64 _len) const {
    return null() || _pos >= m_data->size ? bytes() : 0 == _pos && _len >= m_data->size ? *this
                                                                                        : bytes(data() + _pos, _len == max_uint_64 || m_data->size < _pos + _len ? m_data->size - _pos : _len);
}

bytes_view alx::bytes::mid_view(uint_64 _pos, uint_64 _len) const noexcept {
    return null() || _pos >= m_data->size ? bytes_view() : bytes_view(*this, _pos, _len == max_uint_64 || m_data->size < _pos + _len ? m_data->size - _pos : _len);
}

bytes alx::bytes::left(uint_64 _len) const {
    return null() ? bytes() : _len >= m_data->size ? *this
                                                   : bytes(data(), alx::min_value(_len, size()));
}

bytes_view alx::bytes::left_view(uint_64 _len) const noexcept {
    return null() ? bytes_view() : bytes_view(*this, 0, _len);
}

bytes alx::bytes::right(uint_64 _len) const {
    return null() ? bytes() : _len >= m_data->size ? *this
                                                   : bytes(end() - _len, _len);
}

bytes_view alx::bytes::right_view(uint_64 _len) const noexcept {
    return null() ? bytes_view() : _len >= m_data->size ? bytes_view(*this)
                                                        : bytes_view(*this, m_data->size - _len, _len);
}
