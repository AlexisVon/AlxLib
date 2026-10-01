/*****************************************************************/ /**
 * \file   gt_aplatform.cpp
 * \brief  Unit tests for process_ctrl: soft/hard kill, reaping and exit status
 *
 * \author alexis
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "aplatform.h"
#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <mutex>
#include <thread>

using namespace alx;

#ifndef _WIN32
namespace {

    char proc_state(uint_32 _pid) {
        std::ifstream file("/proc/" + std::to_string(_pid) + "/stat");
        std::string line;
        if (!std::getline(file, line)) return '\0';
        size_t pos = line.rfind(')');
        return pos != std::string::npos && pos + 2 < line.size() ? line[pos + 2] : '\0';
    }

    char wait_proc_state(uint_32 _pid, char _want) {
        for (int i = 0; i < 400 && _want != proc_state(_pid); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return proc_state(_pid);
    }
}

TEST(gt_aplatform, current_describes_self) {
    process_info self = process_info::current();
    EXPECT_EQ(self.pid, (uint_32)::getpid());
    EXPECT_EQ(self.ppid, (uint_32)::getppid());
    EXPECT_GE(self.thread_cnt, 1u);
    EXPECT_NE(self.path.find("alxcore_test"), std::string::npos);
}

TEST(gt_aplatform, find_process_by_pid_and_name) {
    process_info self = process_info::current();

    std::vector<process_info> by_pid = process_info::find_process(self.pid);
    ASSERT_EQ(by_pid.size(), 1u);
    EXPECT_EQ(by_pid.front().path, self.path);
    EXPECT_EQ(by_pid.front().ppid, self.ppid);

    std::vector<process_info> by_name = process_info::find_process("alxcore_test");
    bool found = false;
    for (const auto& info : by_name) {
        EXPECT_EQ(info.path.substr(info.path.find_last_of('/') + 1), "alxcore_test");
        if (info.pid == self.pid) found = true;
    }
    EXPECT_TRUE(found);
}

TEST(gt_aplatform, find_process_accepts_a_full_path) {
    process_info self = process_info::current();
    ASSERT_FALSE(self.path.empty());

    bool found = false;
    for (const auto& info : process_info::find_process(self.path)) {
        if (info.pid == self.pid) found = true;
    }
    EXPECT_TRUE(found);
    EXPECT_TRUE(process_info::find_process("/no/such/program").empty());
}

TEST(gt_aplatform, find_child_process) {
    process_ctrl ctrl("/bin/sleep");
    ASSERT_TRUE(ctrl.start({"30"}));

    std::vector<process_info> children = process_info::find_child_process((uint_32)::getpid());
    bool found = false;
    for (const auto& info : children) {
        if (info.pid == ctrl.pid()) found = true;
    }
    EXPECT_TRUE(found);
    EXPECT_TRUE(ctrl.kill());
}

TEST(gt_aplatform, process_without_executable_has_empty_path) {
    std::vector<process_info> list = process_info::find_process(2u);
    if (list.empty()) GTEST_SKIP();
    EXPECT_TRUE(list.front().path.empty());
    EXPECT_GE(list.front().thread_cnt, 1u);
}

TEST(gt_aplatform, path_is_the_resolved_executable) {
    const std::string link = "/tmp/alx-gt-link-sleep";
    ::unlink(link.c_str());
    ASSERT_EQ(0, ::symlink("/bin/sleep", link.c_str()));

    process_ctrl ctrl(link);
    ASSERT_TRUE(ctrl.start({"30"}));

    const std::string expect = std::filesystem::canonical("/bin/sleep").string();
    std::string path;
    for (int i = 0; i < 400 && path != expect; ++i) {
        std::vector<process_info> list = process_info::find_process(ctrl.pid());
        if (1 == list.size()) path = list.front().path;
        if (path != expect) std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    EXPECT_EQ(path, expect);

    EXPECT_TRUE(ctrl.kill());
    ::unlink(link.c_str());
}

TEST(gt_aplatform, comm_with_space_does_not_break_the_scan) {

    process_ctrl ctrl("/bin/sh");
    ASSERT_TRUE(ctrl.start({"-c", "echo 'my prog' > /proc/self/comm; sleep 30; :"}));

    std::string comm;
    for (int i = 0; i < 400 && comm != "my prog"; ++i) {
        comm.clear();
        std::ifstream file("/proc/" + std::to_string(ctrl.pid()) + "/comm");
        std::getline(file, comm);
        if (comm != "my prog") std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    ASSERT_EQ(comm, "my prog");

    ASSERT_NO_THROW(process_info::find_process([](const process_info&) { return true; }));
    std::vector<process_info> list = process_info::find_process(ctrl.pid());
    ASSERT_EQ(list.size(), 1u);

    std::string name = list.front().path.substr(list.front().path.find_last_of('/') + 1);
    ASSERT_FALSE(name.empty());
    bool found = false;
    for (const auto& info : process_info::find_process(name)) {
        if (info.pid == ctrl.pid()) found = true;
    }
    EXPECT_TRUE(found);

    EXPECT_TRUE(ctrl.kill());
}

TEST(gt_aplatform, memory_limit_is_visible_to_the_child) {
    process_ctrl ctrl("/bin/sh");
    process_ctrl::process_limits limits;
    limits.mem_bytes = 64ull * 1024 * 1024;

    std::mutex mtx;
    std::string out;
    ctrl.sig_output.connect([&](const std::string& _string) {
        std::lock_guard<std::mutex> lock(mtx);
        out += _string;
    });

    ASSERT_TRUE(ctrl.start({"-c", "ulimit -v"}, limits));
    EXPECT_TRUE(ctrl.wait(2000));
    for (int i = 0; i < 400 && out.find("65536") == std::string::npos; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    {
        std::lock_guard<std::mutex> lock(mtx);
        EXPECT_NE(out.find("65536"), std::string::npos) << "ulimit -v printed: " << out;
    }
    ctrl.kill();
}

TEST(gt_aplatform, memory_limit_cannot_be_raised_by_the_child) {
    process_ctrl ctrl("/bin/sh");
    process_ctrl::process_limits limits;
    limits.mem_bytes = 64ull * 1024 * 1024;

    std::mutex mtx;
    std::string err;
    ctrl.sig_errput.connect([&](const std::string& _string) {
        std::lock_guard<std::mutex> lock(mtx);
        err += _string;
    });

    ASSERT_TRUE(ctrl.start({"-c", "ulimit -v unlimited"}, limits));
    EXPECT_TRUE(ctrl.wait(2000));
    for (int i = 0; i < 400 && err.empty(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    {
        std::lock_guard<std::mutex> lock(mtx);
        EXPECT_NE(err.find("not permitted"), std::string::npos) << "stderr was: " << err;
    }
    EXPECT_NE(ctrl.status().exit_code, 0);
    ctrl.kill();
}

TEST(gt_aplatform, memory_wall_stops_a_big_allocation) {
    const std::string alloc = "x=$(head -c 134217728 /dev/zero | tr '\\0' x); echo done";

    {
        process_ctrl ctrl("/bin/sh");
        std::mutex mtx;
        std::string out;
        ctrl.sig_output.connect([&](const std::string& _string) {
            std::lock_guard<std::mutex> lock(mtx);
            out += _string;
        });
        ASSERT_TRUE(ctrl.start({"-c", alloc}));
        EXPECT_TRUE(ctrl.wait(30000));
        for (int i = 0; i < 400 && out.find("done") == std::string::npos; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        {
            std::lock_guard<std::mutex> lock(mtx);
            EXPECT_NE(out.find("done"), std::string::npos) << "control run: " << out;
        }
        EXPECT_EQ(ctrl.status().exit_code, 0);
        ctrl.kill();
    }

    {
        process_ctrl ctrl("/bin/sh");
        process_ctrl::process_limits limits;
        limits.mem_bytes = 64ull * 1024 * 1024;

        std::mutex mtx;
        std::string out;
        ctrl.sig_output.connect([&](const std::string& _string) {
            std::lock_guard<std::mutex> lock(mtx);
            out += _string;
        });
        ASSERT_TRUE(ctrl.start({"-c", alloc}, limits));
        EXPECT_TRUE(ctrl.wait(30000));
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        {
            std::lock_guard<std::mutex> lock(mtx);
            EXPECT_EQ(out.find("done"), std::string::npos) << "walled run: " << out;
        }
        EXPECT_NE(ctrl.status().exit_code, 0);
        ctrl.kill();
    }
}

TEST(gt_aplatform, start_fails_when_the_child_cannot_exec) {
    process_ctrl::process_limits limits;
    limits.mem_bytes = 64ull * 1024 * 1024;

    process_ctrl plain("/no/such/alx-gt-binary");
    EXPECT_FALSE(plain.start({}));
    EXPECT_EQ(plain.pid(), 0u);
    EXPECT_FALSE(plain.status().exited);
    EXPECT_FALSE(plain.wait(0));

    process_ctrl walled("/no/such/alx-gt-binary");
    EXPECT_FALSE(walled.start({}, limits));
    EXPECT_EQ(walled.pid(), 0u);
}

TEST(gt_aplatform, idle_has_no_child) {
    process_ctrl ctrl("/bin/sleep");
    EXPECT_EQ(ctrl.pid(), 0u);
    EXPECT_FALSE(ctrl.terminate());
    EXPECT_FALSE(ctrl.kill());
    EXPECT_FALSE(ctrl.status().exited);
    EXPECT_FALSE(ctrl.wait(0));
}

TEST(gt_aplatform, start_reports_exit_code) {
    process_ctrl ctrl("/bin/sh");
    ASSERT_TRUE(ctrl.start({"-c", "exit 3"}));
    ASSERT_NE(ctrl.pid(), 0u);

    EXPECT_TRUE(ctrl.wait(2000));
    EXPECT_TRUE(ctrl.status().exited);
    EXPECT_FALSE(ctrl.status().signaled);
    EXPECT_EQ(ctrl.status().exit_code, 3);
    EXPECT_EQ(ctrl.status().term_sig, 0);

    EXPECT_NE(ctrl.pid(), 0u);
    EXPECT_FALSE(ctrl.kill());
    EXPECT_EQ(ctrl.pid(), 0u);
    EXPECT_TRUE(ctrl.status().exited);
    EXPECT_EQ(ctrl.status().exit_code, 3);
}

TEST(gt_aplatform, kill_reports_the_exit_code_of_a_child_that_died_on_its_own) {
    process_ctrl ctrl("/bin/sh");
    ASSERT_TRUE(ctrl.start({"-c", "exit 4"}));
    EXPECT_EQ(wait_proc_state(ctrl.pid(), 'Z'), 'Z');

    EXPECT_FALSE(ctrl.kill());
    EXPECT_EQ(ctrl.pid(), 0u);
    EXPECT_TRUE(ctrl.status().exited);
    EXPECT_EQ(ctrl.status().exit_code, 4);
}

TEST(gt_aplatform, nothing_reaps_until_asked) {
    process_ctrl ctrl("/bin/sh");
    ASSERT_TRUE(ctrl.start({"-c", "exit 5"}));
    uint_32 pid = ctrl.pid();

    EXPECT_EQ(wait_proc_state(pid, 'Z'), 'Z');
    EXPECT_TRUE(ctrl.status().exited);
    EXPECT_EQ(ctrl.status().exit_code, 5);
    EXPECT_EQ(proc_state(pid), '\0');
}

TEST(gt_aplatform, kill_reports_signal) {
    process_ctrl ctrl("/bin/sleep");
    ASSERT_TRUE(ctrl.start({"30"}));

    EXPECT_TRUE(ctrl.kill());
    EXPECT_EQ(ctrl.pid(), 0u);
    EXPECT_TRUE(ctrl.status().exited);
    EXPECT_TRUE(ctrl.status().signaled);
    EXPECT_EQ(ctrl.status().term_sig, SIGKILL);
    EXPECT_FALSE(ctrl.kill());
}

TEST(gt_aplatform, terminate_reports_signal) {
    process_ctrl ctrl("/bin/sleep");
    ASSERT_TRUE(ctrl.start({"30"}));

    EXPECT_TRUE(ctrl.terminate());
    EXPECT_TRUE(ctrl.wait(2000));
    EXPECT_TRUE(ctrl.status().signaled);
    EXPECT_EQ(ctrl.status().term_sig, SIGTERM);
    EXPECT_FALSE(ctrl.terminate());
}

TEST(gt_aplatform, crash_reports_signal) {
    process_ctrl ctrl("/bin/sh");
    ASSERT_TRUE(ctrl.start({"-c", "kill -ABRT $$"}));

    EXPECT_TRUE(ctrl.wait(2000));
    EXPECT_TRUE(ctrl.status().exited);
    EXPECT_TRUE(ctrl.status().signaled);
    EXPECT_EQ(ctrl.status().term_sig, SIGABRT);
}

TEST(gt_aplatform, wait_times_out) {
    process_ctrl ctrl("/bin/sleep");
    ASSERT_TRUE(ctrl.start({"30"}));

    auto begin = std::chrono::steady_clock::now();
    EXPECT_FALSE(ctrl.wait(200));
    EXPECT_GE(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin).count(), 200);
    EXPECT_FALSE(ctrl.status().exited);

    EXPECT_TRUE(ctrl.kill());
}

TEST(gt_aplatform, input_to_dead_child_returns_false) {
    process_ctrl ctrl("/bin/sh");
    ASSERT_TRUE(ctrl.start({"-c", "exit 0"}));
    uint_32 pid = ctrl.pid();
    ASSERT_EQ(wait_proc_state(pid, 'Z'), 'Z');

    EXPECT_FALSE(ctrl.input(std::string(64 * 1024, 'x')));
    EXPECT_FALSE(ctrl.input("tail"));
    EXPECT_EQ(proc_state(pid), '\0');
}

TEST(gt_aplatform, restart_clears_status) {
    process_ctrl ctrl("/bin/sh");
    ASSERT_TRUE(ctrl.start({"-c", "exit 7"}));
    EXPECT_TRUE(ctrl.wait(2000));
    EXPECT_EQ(ctrl.status().exit_code, 7);

    ASSERT_TRUE(ctrl.start({"-c", "sleep 30"}));
    EXPECT_NE(ctrl.pid(), 0u);
    EXPECT_FALSE(ctrl.status().exited);
    EXPECT_TRUE(ctrl.kill());
}

TEST(gt_aplatform, restarts_do_not_leak_descriptors) {
    auto fd_count = []() {
        size_t count = 0;
        for (const auto& entry : std::filesystem::directory_iterator("/proc/self/fd")) {
            (void) entry;
            ++count;
        }
        return count;
    };

    process_ctrl ctrl("/bin/sh");
    ASSERT_TRUE(ctrl.start({"-c", "exit 0"}));
    EXPECT_TRUE(ctrl.wait(2000));
    size_t before = fd_count();

    for (int i = 0; i < 50; ++i) {
        ASSERT_TRUE(ctrl.start({"-c", "exit 0"}));
        EXPECT_TRUE(ctrl.wait(2000));
    }
    EXPECT_EQ(fd_count(), before);
}

TEST(gt_aplatform, output_is_forwarded_raw) {
    process_ctrl ctrl("/bin/sh");
    std::mutex mtx;
    std::string out;
    ctrl.sig_output.connect([&](const std::string& _string) {
        std::lock_guard<std::mutex> lock(mtx);
        out += _string;
    });
    ASSERT_TRUE(ctrl.start({"-c", "echo hello"}));

    bool got_line = false;
    for (int i = 0; i < 400 && !got_line; ++i) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            got_line = out.find('\n') != std::string::npos;
        }
        if (!got_line) std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    {
        std::lock_guard<std::mutex> lock(mtx);
        EXPECT_NE(out.find("hello"), std::string::npos);
        EXPECT_NE(out.find('\n'), std::string::npos);
    }
    ctrl.kill();
}

TEST(gt_aplatform, destructor_reaps) {
    uint_32 pid = 0;
    {
        process_ctrl ctrl("/bin/sleep");
        ASSERT_TRUE(ctrl.start({"30"}));
        pid = ctrl.pid();
        ASSERT_NE(proc_state(pid), '\0');
    }
    EXPECT_EQ(wait_proc_state(pid, '\0'), '\0');
}

TEST(gt_aplatform, output_done_marks_the_end_of_the_stream) {
    process_ctrl ctrl("/bin/sh");
    std::mutex mtx;
    size_t out = 0;
    ctrl.sig_output.connect([&](const std::string& _string) {
        std::lock_guard<std::mutex> lock(mtx);
        out += _string.size();
    });
    EXPECT_TRUE(ctrl.output_done());

    const char* child = "sleep 0.2; i=0; while [ $i -lt 10000 ]; do printf x; i=$((i+1)); done; echo";
    ASSERT_TRUE(ctrl.start({"-c", child}));
    EXPECT_FALSE(ctrl.output_done());

    ASSERT_TRUE(ctrl.wait(5000));
    EXPECT_TRUE(ctrl.output_done(5000));
    {
        std::lock_guard<std::mutex> lock(mtx);
        EXPECT_EQ(out, 10001u);
    }
    ctrl.kill();
}

TEST(gt_aplatform, output_done_is_set_by_kill) {
    process_ctrl ctrl("/bin/sleep");
    ASSERT_TRUE(ctrl.start({"30"}));
    EXPECT_FALSE(ctrl.output_done());

    EXPECT_TRUE(ctrl.kill());
    EXPECT_TRUE(ctrl.output_done());

    ASSERT_TRUE(ctrl.start({"30"}));
    EXPECT_FALSE(ctrl.output_done());
    ctrl.kill();
}
#endif
