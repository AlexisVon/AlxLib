/*****************************************************************/ /**
 * \file   aes_bench.cpp
 * \brief  AES benchmark: the eight-block batch paths against the single-block ones
 *
 * Measures AES-256 over one buffer for ECB, CBC, CTR and GCM. Each operation builds a
 * fresh mode object, so key expansion (and GCM's power table) is inside the sample, the
 * way one fpacker block pays it. The buffer is reused and its content is irrelevant to
 * the rate.
 *
 * Methodology: IQR mean over the samples (tools/bench_common.h); comparing two revisions
 * means running this same binary against each build, alternating rounds.
 *
 * \author alexis
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "aaes.h"
#include "bench_common.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace alx;

int main(int argc, char** argv) {
#ifdef GIT_COMMIT
    printf("build %s\n", GIT_COMMIT);
#endif
    const unsigned long long mib = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 64;
    const int samples = argc > 2 ? std::atoi(argv[2]) : 15;
    const uint_64 size = (uint_64) mib * 1024 * 1024;
    if (0 == size) {
        printf("usage: aes_bench [mib] [samples]\n");
        return 2;
    }

    bytes buf(size, 0x5a);
    uint_8 key[32], iv[16], iv12[12];
    memset(key, 0x5a, sizeof(key));
    memset(iv, 0xa5, sizeof(iv));
    memset(iv12, 0x3c, sizeof(iv12));
    printf("(buffer %llu MiB, %d samples, AES-256)\n", mib, samples);

    bench::run_rate("aes_ecb encrypt", size, [&] { aes_ecb<256> alg(key); alg.encrypt(buf.data(), size); }, samples);
    bench::run_rate("aes_ecb decrypt", size, [&] { aes_ecb<256> alg(key); alg.decrypt(buf.data(), size); }, samples);
    bench::run_rate("aes_cbc encrypt", size, [&] { aes_cbc<256> alg(key, iv); alg.encrypt(buf.data(), size); }, samples);
    bench::run_rate("aes_cbc decrypt", size, [&] { aes_cbc<256> alg(key, iv); alg.decrypt(buf.data(), size); }, samples);
    bench::run_rate("aes_ctr encrypt", size, [&] { aes_ctr<256> alg(key, iv); alg.encrypt(buf.data(), size); }, samples);
    bench::run_rate(
        "aes_gcm encrypt", size,
        [&] {
            aes_gcm<256> alg(key, iv12);
            alg.start(nullptr, 0);
            alg.encrypt(buf.data(), size);
            bytes tag;
            alg.finish(tag);
        },
        samples);
    return 0;
}
