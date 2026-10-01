/*****************************************************************/ /**
 * \file   bench_common.h
 * \brief  Shared sampling helpers for the tools/*_bench.cpp programs
 *
 * \author alexis
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#ifndef _ALX_TOOLS_BENCH_COMMON_H_
#define _ALX_TOOLS_BENCH_COMMON_H_

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <functional>
#include <vector>

namespace bench {
    using clk = std::chrono::steady_clock;

    inline double iqr_mean(std::vector<double>& _samples) {
        std::sort(_samples.begin(), _samples.end());
        const size_t from = _samples.size() / 4;
        const size_t to = _samples.size() - from;
        double sum = 0.0;
        for (size_t i = from; i < to; i++) sum += _samples[i];
        return 0.0 == sum ? 0.0 : sum / (double) (to - from);
    }

    inline void run(const char* _name, const std::function<void()>& _op, int _samples = 21) {
        _op();
        std::vector<double> taken;
        taken.reserve((size_t) _samples);
        for (int i = 0; i < _samples; i++) {
            const auto start = clk::now();
            _op();
            taken.push_back(std::chrono::duration<double, std::milli>(clk::now() - start).count());
        }
        printf("%-34s %10.4f ms/op\n", _name, iqr_mean(taken));
        fflush(stdout);
    }
}

#endif
