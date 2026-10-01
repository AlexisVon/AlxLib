// Copyright (c) 2026 AlexisVon
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

static const std::string kTmpDir = "tmp-alxscpt-gtest";

class AlxScptEnv : public ::testing::Environment {
public:
    void SetUp() override {

        fs::remove_all(kTmpDir);
        fs::create_directories(kTmpDir);
    }

    void TearDown() override {
        fs::remove_all(kTmpDir);
    }
};

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    ::testing::AddGlobalTestEnvironment(new AlxScptEnv);
    return RUN_ALL_TESTS();
}
