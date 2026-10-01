// Copyright (c) 2026 AlexisVon
// SPDX-License-Identifier: MIT

#pragma once

#include "atransmit.h"
#include <atomic>
#include <functional>

namespace alx {
    namespace test {

        class MockTransmit : public transmit {
        public:
            MockTransmit()
                : transmit("mock:0") {}

            std::atomic<bool> open_result{true};
            std::atomic<bool> is_open_result{true};
            std::atomic<bool> send_result{true};
            std::atomic<uint_64> connect_num_result{1};

            bytes last_sent;
            uint_64 last_sent_loc{0};

            std::function<void(const bytes_view&, uint_64)> on_send;

            bool open() override { return open_result; }

            void close(uint_64 = 0) override {
                connect_num_result = 0;
                is_open_result = false;
            }

            bool is_open() override { return is_open_result; }
            uint_64 connect_num() override { return connect_num_result; }

            bool bytes_send(const void* _data, const uint_64 _size, uint_64 _loc = 0) override {
                if (!send_result) return false;
                last_sent = bytes((const char*) _data, _size);
                last_sent_loc = _loc;
                if (on_send) on_send(last_sent, _loc);
                return true;
            }

            void inject_recv(const bytes_view& _data, uint_64 _loc = 0) {
                bytes_recv.exec(_data, _loc);
            }

            void inject_connect(uint_64 _loc, bool _connected, const char* _msg = "mock") {
                trans_mesg.exec(_loc, _connected, _msg);
            }
        };

    }
}
