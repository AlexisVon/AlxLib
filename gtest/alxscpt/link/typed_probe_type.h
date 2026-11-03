// Copyright (c) 2026 AlexisVon
#ifndef _ALX_TEST_TYPED_PROBE_TYPE_H_
#define _ALX_TEST_TYPED_PROBE_TYPE_H_

namespace alx_test {

    // Shared by the typed_probe fixtures so both modules hash the same type id
    struct probe_box {
        int w = 640;
        int h = 480;
    };

}

#endif
