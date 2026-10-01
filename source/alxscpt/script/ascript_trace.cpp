/*****************************************************************/ /**
 * \file   ascript_trace.cpp
 * \brief  Call-stack trace compression — repeated-segment folding
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#include "ascript_trace.h"
#include <algorithm>
#include <unordered_map>

namespace alx {
    namespace script {

        namespace {

            using id_seq = std::vector<size_t>;

            struct trace_window {
                size_t start;
                size_t period;
                size_t repeat;
                size_t cover;
            };

            void collect_windows(const id_seq& _ids, size_t _lo, size_t _hi,
                                 std::vector<trace_window>& _out) {
                for (size_t L = 1; L <= (_hi - _lo) / 2; L++) {
                    size_t j = _lo;
                    while (j + L < _hi) {
                        if (_ids[j] != _ids[j + L]) {
                            j++;
                            continue;
                        }
                        size_t s = j;
                        while (j + L < _hi && _ids[j] == _ids[j + L]) j++;
                        // A run of r equal L-shifted pairs is r / L + 1 periods: the trailing partial period counts too
                        size_t K = (j - s + L) / L;
                        if (K >= 2) {

                            // The same cycle in its other rotation -- which alignment folds the period better is not known here
                            _out.push_back(trace_window{s, L, K, L * K});
                            size_t tail = j + L - L * K;
                            if (tail != s) _out.push_back(trace_window{tail, L, K, L * K});
                        }
                        j++;
                    }
                }
            }

            std::vector<trace_item> trace_decompose(const id_seq& _ids,
                                                    const std::vector<std::string>& _texts,
                                                    size_t _lo, size_t _hi) {
                std::vector<trace_window> windows;
                collect_windows(_ids, _lo, _hi, windows);

                std::vector<trace_item> out;
                size_t cursor = _lo;
                if (!windows.empty()) {

                    // Longest cover first: an overlapped window still claims its free part, and what it lost lies inside a decomposed period
                    std::sort(windows.begin(), windows.end(),
                              [](const trace_window& _a, const trace_window& _b) {
                                  if (_a.cover != _b.cover) return _a.cover > _b.cover;
                                  if (_a.start + _a.cover != _b.start + _b.cover)
                                      return _a.start + _a.cover > _b.start + _b.cover;
                                  if (_a.start != _b.start) return _a.start < _b.start;
                                  return _a.period < _b.period;
                              });
                    std::vector<bool> taken(_hi - _lo, false);
                    std::vector<trace_window> picked;
                    for (auto& w : windows) {

                        // Claim the free stretch only in whole, window-aligned periods, and only if two or more of them remain
                        size_t e = w.start + w.cover;
                        size_t k = w.start;
                        while (k < e) {
                            while (k < e && taken[k - _lo]) k++;
                            size_t free_end = k;
                            while (free_end < e && !taken[free_end - _lo]) free_end++;
                            size_t off = (k - w.start + w.period - 1) / w.period * w.period;
                            size_t from = w.start + off;
                            size_t cover = free_end > from ? (free_end - from) / w.period * w.period : 0;
                            if (cover >= 2 * w.period) {
                                for (size_t t = from; t < from + cover; t++) taken[t - _lo] = true;
                                picked.push_back(
                                    trace_window{from, w.period, cover / w.period, cover});
                            }
                            k = free_end;
                        }
                    }
                    std::sort(picked.begin(), picked.end(),
                              [](const trace_window& _a, const trace_window& _b) {
                                  return _a.start < _b.start;
                              });
                    for (auto& w : picked) {
                        for (size_t k = cursor; k < w.start; k++)
                            out.push_back(trace_item{_texts[_ids[k]], 1, {}});
                        // Only one period is decomposed -- the other repeats are copies by construction, and a group item carries no text
                        out.push_back(trace_item{
                            std::string(), w.repeat,
                            trace_decompose(_ids, _texts, w.start, w.start + w.period)});
                        cursor = w.start + w.cover;
                    }
                }

                for (size_t k = cursor; k < _hi; k++)
                    out.push_back(trace_item{_texts[_ids[k]], 1, {}});
                return out;
            }

        }

        std::vector<trace_item> compress_trace(const std::vector<std::string>& _seq) {
            size_t n = _seq.size();

            std::vector<std::string> texts;
            id_seq ids(n);
            // Equal text shares an id: identity is the map's string equality, so a hash collision cannot merge two frames
            std::unordered_map<std::string, size_t> interned;
            interned.reserve(n * 2);
            for (size_t k = 0; k < n; k++) {
                auto [it, inserted] = interned.emplace(_seq[k], texts.size());
                if (inserted) texts.push_back(_seq[k]);
                ids[k] = it->second;
            }

            return trace_decompose(ids, texts, 0, n);
        }

        void format_trace(const std::vector<trace_item>& _items, std::string& _out,
                          const std::string& _indent) {
            for (auto& item : _items) {
                // A group prints its count once, then its contents once: the repeats are never expanded
                if (item.children.empty()) {
                    _out += _indent + "   | " + item.text + "\n";
                } else {
                    _out += _indent + "   | (" + std::to_string(item.repeat) + "x)\n";
                    format_trace(item.children, _out, _indent + "  ");
                }
            }
        }

    }
}
