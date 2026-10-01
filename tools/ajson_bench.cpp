/*****************************************************************/ /**
 * \file   ajson_bench.cpp
 * \brief  JSON serialization benchmark: the paths a sink change must not slow down
 *
 * \author alexis
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "ajson.h"
#include "astream.h"
#include "bench_common.h"
#include <cstdio>
#include <string>

class bounded_ostream : public alx::ostream {
public:
    explicit bounded_ostream(alx::uint_64 _cap)
        : cap_(_cap) {}
    bool append(const void* _ptr, alx::uint_64 _size) override {
        if (size_ + _size > cap_) return false;
        buff_.append((const char*) _ptr, (size_t) _size);
        size_ += _size;
        return true;
    }
    bool flush() override { return true; }
    void reset() override {
        buff_.clear();
        size_ = 0;
    }
    alx::uint_64 total() const override { return size_; }

private:
    alx::uint_64 cap_;
    alx::uint_64 size_{0};
    std::string buff_;
};

int main() {
#ifdef GIT_COMMIT
    printf("build %s\n", GIT_COMMIT);
#endif
    alx::json_object doc;
    for (int i = 0; i < 20000; i++) {
        const std::string key = "key_" + std::to_string(i);
        switch (i % 5) {
        case 0: doc.insert(key, alx::json_value((long long) i)); break;
        case 1: doc.insert(key, alx::json_value(std::string("value ") + std::to_string(i))); break;
        case 2: doc.insert(key, alx::json_value(1.5 * i)); break;
        case 3: doc.insert(key, alx::json_value(i % 2 == 0)); break;
        default: doc.insert(key, alx::json_value(std::string("esc \"q\" \\b \n\t") + std::to_string(i))); break;
        }
    }

    std::string plain(1 << 20, 'x');
    std::string mixed;
    mixed.reserve(1 << 20);
    for (size_t i = 0; i < (1u << 20); i++) mixed += (i % 100 == 0) ? "\"" : "y";

    size_t sink = 0;
    bench::run("to_json(const&, compact)", [&] { sink += alx::json_doc::to_json(doc, true).size(); });
    bench::run("to_json(const&, pretty)", [&] { sink += alx::json_doc::to_json(doc, false).size(); });

    bounded_ostream stream(1 << 24);
    bench::run("to_json(const&, stream)", [&] {
        stream.reset();
        if (!alx::json_doc::to_json(doc, stream, true)) sink++;
    });

    std::string out;
    bench::run("escape 1MB plain (append)", [&] { alx::json_doc::escape(plain, out, 0); });
    bench::run("escape 1MB mixed (append)", [&] { alx::json_doc::escape(mixed, out, 0); });
    bench::run("escape 1MB mixed (in place)", [&] {
        std::string s = mixed;
        alx::json_doc::escape(s, s, 0);
        sink += s.size();
    });
    bench::run("descape 1MB mixed (in place)", [&] {
        std::string s = mixed;
        alx::json_doc::descape(s, s, 0);
        sink += s.size();
    });

    printf("(sink %zu)\n", sink);
    return 0;
}
