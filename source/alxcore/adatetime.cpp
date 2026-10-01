/*****************************************************************/ /**
 * \file   adatetime.cpp
 * \brief  Date and time processing
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "adatetime.h"

#include <iomanip>
#include <sstream>

using namespace alx;

alx::datetime::datetime(
    uint_16 _y, uint_16 _m, uint_16 _d,
    uint_16 _h, uint_16 _min, uint_16 _s,
    uint_16 _ms, uint_16 _us) {
    std::tm _tm{};
    _tm.tm_year = _y - 1900;
    _tm.tm_mon = _m - 1;
    _tm.tm_mday = _d;
    _tm.tm_hour = _h;
    _tm.tm_min = _min;
    _tm.tm_sec = _s;
    time_t sec = std::mktime(&_tm);
    // mktime() takes any date, but from_time_t() holds only 1677-2262 at nanosecond resolution; outside it wraps
    m_tp = clock::from_time_t(sec) + std::chrono::milliseconds(_ms) + std::chrono::microseconds(_us);
}

uint_16 alx::datetime::millisecond() const {
    auto duration = m_tp.time_since_epoch();
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(duration);
    return uint_16((std::chrono::duration_cast<std::chrono::milliseconds>(duration - secs)).count());
}

uint_32 alx::datetime::microsecond() const {
    auto duration = m_tp.time_since_epoch();
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(duration);
    return uint_32((std::chrono::duration_cast<std::chrono::microseconds>(duration - secs)).count());
}

std::string alx::datetime::to_string() const {
    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(4) << year() << '-'
        << std::setw(2) << month() << '-'
        << std::setw(2) << day() << ' '
        << std::setw(2) << hour() << ':'
        << std::setw(2) << minute() << ':'
        << std::setw(2) << second() << '.'
        << std::setw(3) << millisecond();
    return oss.str();
}

uint_64 alx::datetime::elapsed_ms() const {
    auto now = clock::now();
    auto duration = now - m_tp;
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

uint_64 alx::datetime::elapsed_us() const {
    auto now = clock::now();
    auto duration = now - m_tp;
    return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
}

uint_64 alx::datetime::elapsed_ns() const {
    auto now = clock::now();
    auto duration = now - m_tp;
    return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
}
