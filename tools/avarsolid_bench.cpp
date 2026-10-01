/*****************************************************************/ /**
 * \file   avarsolid_bench.cpp
 * \brief  varsolid (binary varmap) benchmark: the paths a deserializer change touches
 *
 * Layer A (config-sized): a nested map with every scalar kind plus a vector — the
 *                         shape a host parses once and queries by path.
 * Layer B (bulk): 20k entries, serialize / deserialize / validate throughput.
 *
 * Methodology:
 *   - corpora are built once, outside timing; the destination varmap is reused so
 *     its allocation churn does not land in the samples
 *   - IQR mean sampling (tools/bench_common.h); comparing two revisions means
 *     running this same binary against each build, alternating rounds
 *
 * What to watch when touching aserial.h / avarsolid.cpp: to_varmap and get_value
 * are the two entry points that walk raw bytes with r_interpret.
 *
 * \author alexis
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "avarsolid.h"
#include "bench_common.h"
#include <cstdio>
#include <list>
#include <string>

using alx::bytes;
using alx::varmap;
using alx::variant;
using alx::varvec;

static varmap make_config() {
    varmap cfg;
    cfg.insert("name", variant(std::string("sample")));
    cfg.insert("version", variant((long long) 3));
    cfg.insert("ratio", variant(0.75));
    cfg.insert("enabled", variant(true));
    cfg.insert("seed", variant(bytes("0123456789abcdef")));

    varvec ports;
    for (int i = 0; i < 16; i++) ports.push_back(variant((long long) (1000 + i)));
    cfg.insert("ports", variant(ports));

    varmap server;
    server.insert("host", variant(std::string("127.0.0.1")));
    server.insert("port", variant((long long) 8080));
    cfg.insert("server", variant(server));
    return cfg;
}

static varmap make_big() {
    varmap big;
    for (int i = 0; i < 20000; i++) {
        const std::string key = "key_" + std::to_string(i);
        switch (i % 4) {
        case 0: big.insert(key, variant((long long) i)); break;
        case 1: big.insert(key, variant(std::string("value ") + std::to_string(i))); break;
        case 2: big.insert(key, variant(1.5 * i)); break;
        default: big.insert(key, variant(i % 2 == 0)); break;
        }
    }
    return big;
}

int main() {
#ifdef GIT_COMMIT
    printf("build %s\n", GIT_COMMIT);
#endif
    const varmap config = make_config();
    const varmap big = make_big();

    bytes blob_config, blob_big;
    if (!alx::varsolid::to_bytes(config, blob_config) || !alx::varsolid::to_bytes(big, blob_big)) {
        printf("corpus failed to serialize\n");
        return 1;
    }
    printf("(corpus: config %zu bytes, big %zu bytes, %zu keys)\n", (size_t) blob_config.size(), (size_t) blob_big.size(), big.size());

    const std::list<std::string> port_path{"server", "port"};
    size_t sink = 0;
    varmap reused;

    bench::run("to_bytes(config)", [&] { sink += alx::varsolid::to_bytes(config, blob_config) ? 1 : 0; });
    bench::run("to_bytes(big, 20k)", [&] { sink += alx::varsolid::to_bytes(big, blob_big) ? 1 : 0; });

    bytes stream_buff;
    alx::ostream_buff ostm(stream_buff);
    bench::run("to_bytes(big, stream)", [&] {
        ostm.reset();
        sink += alx::varsolid::to_bytes(big, ostm) ? 1 : 0;
    });

    bench::run("to_varmap(big)", [&] { sink += alx::varsolid::to_varmap(blob_big, reused) ? 1 : 0; });
    bench::run("is_valid(big)", [&] { sink += alx::varsolid::is_valid(blob_big) ? 1 : 0; });
    bench::run("get_value(config, path)", [&] { sink += (size_t) alx::varsolid::get_value(blob_config, port_path).to<long long>(); });

    printf("(sink %zu)\n", sink);
    return 0;
}
