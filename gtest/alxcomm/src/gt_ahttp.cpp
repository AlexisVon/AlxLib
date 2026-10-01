/*****************************************************************/ /**
 * \file   gt_ahttp.cpp
 * \brief  Unit tests for HTTP: request/reply serialization, api routing, server/client
 *
 * \author alexis
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "acompress.h"
#include "ahttp.h"
#include "test_mocks.h"

#include <atomic>
#include <chrono>
#include <future>
#include <gtest/gtest.h>
#include <memory>
#include <thread>

using namespace alx;
using namespace alx::http;
using namespace alx::test;

namespace {
    static bool wait_for(std::function<bool()> _cond, int _ms = 10000) {
        for (int i = 0; i < _ms / 50 && !_cond(); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        return _cond();
    }

    bool contains(const bytes& _b, const std::string& _s) {
        return std::string((const char*) _b.data(), _b.size()).find(_s) != std::string::npos;
    }
}

TEST(gt_ahttp, request_to_bytes_get) {
    request req(action::GET, version::V1_1, true);
    req.set_urlwords({"api", "test"}, {});
    bytes wire = req.to_bytes();
    EXPECT_TRUE(contains(wire, "GET /api/test HTTP/1.1"));
    EXPECT_TRUE(contains(wire, "keep-alive"));
}

TEST(gt_ahttp, request_to_bytes_post_json) {
    request req(action::POST, version::V1_1, true);
    req.set_urlwords({"submit"}, {});
    req.set_content(mime::app::json, bytes_view(bytes("{\"k\":1}")));

    bytes wire = req.to_bytes();
    EXPECT_TRUE(contains(wire, "POST /submit HTTP/1.1"));
    EXPECT_TRUE(contains(wire, "application/json"));
    EXPECT_TRUE(contains(wire, "Content-Length: 7"));
    EXPECT_TRUE(contains(wire, "{\"k\":1}"));
}

TEST(gt_ahttp, request_to_bytes_query_params) {
    request req(action::GET, version::V1_1, true);
    req.set_urlwords({"/", "search"}, {{"q", "hello"}, {"page", "1"}});

    bytes wire = req.to_bytes();
    EXPECT_TRUE(contains(wire, "q=hello"));
    EXPECT_TRUE(contains(wire, "page=1"));
}

TEST(gt_ahttp, request_to_bytes_percent_encoding_is_uppercase) {
    request req(action::GET, version::V1_1, true);
    req.set_urlwords({"/", "search"}, {{"q", "你好"}, {"s", "a b"}});

    bytes wire = req.to_bytes();

    EXPECT_TRUE(contains(wire, "q=%E4%BD%A0%E5%A5%BD"));
    EXPECT_TRUE(contains(wire, "s=a%20b"));
}

TEST(gt_ahttp, request_to_bytes_keepalive_close) {
    request req(action::GET, version::V1_0, false);
    req.set_urlwords({"/"}, {});
    bytes wire = req.to_bytes();
    EXPECT_TRUE(contains(wire, "HTTP/1.0"));
    EXPECT_TRUE(contains(wire, "Connection: close"));
}

TEST(gt_ahttp, reply_to_bytes_200) {
    reply r(version::V1_1, state::succ::OK, true);
    r.set_content(mime::txt::plain, bytes_view(bytes("OK")));
    bytes wire = r.to_bytes();
    EXPECT_TRUE(contains(wire, "200 OK"));
    EXPECT_TRUE(contains(wire, "OK"));
}

TEST(gt_ahttp, reply_to_bytes_404) {
    reply r(version::V1_1, state::cerr::NOT_FOUND, true);
    r.set_content(mime::txt::plain, bytes_view(bytes("Not Found")));
    bytes wire = r.to_bytes();
    EXPECT_TRUE(contains(wire, "404 Not Found"));
}

TEST(gt_ahttp, api_root_handler) {
    bool called = false;
    api root([&](const request&) -> reply {
        called = true;
        return reply(version::V1_1, state::succ::OK, false);
    });
    request req(action::GET, version::V1_1, true);
    reply r = root.exec(req, {});
    EXPECT_TRUE(called);
}

TEST(gt_ahttp, api_nested) {
    bool called = false;
    api root(nullptr);
    api* a = new api(nullptr);
    api* b = new api([&](const request&) -> reply {
        called = true;
        return reply(version::V1_1, state::succ::OK, false);
    });
    a->insert(b, "baz");
    root.insert(a, "bar");
    request req(action::GET, version::V1_1, true);
    root.exec(req, {"bar", "baz"});
    EXPECT_TRUE(called);
}

TEST(gt_ahttp, api_unmatched) {
    api root([](const request&) -> reply {
        return reply(version::V1_1, state::cerr::NOT_FOUND, false);
    });
    request req(action::GET, version::V1_1, true);
    reply r = root.exec(req, {"nonexistent"});
    EXPECT_EQ(r.get_state(), state::cerr::NOT_FOUND);
}

TEST(gt_ahttp, server_parse_get) {
    std::atomic<bool> called{false};
    action act = action::NONE;
    std::thread t([&] {
        auto* mock = new MockTransmit();
        api* root = new api([&](const request& _req) -> reply {
            called = true;
            act = _req.get_action();
            return reply(version::V1_1, state::succ::OK, true);
        });
        http::server srv(mock, root, 1, 16);
        srv.open();

        std::string raw = "GET / HTTP/1.1\r\nHost: test\r\n\r\n";
        mock->inject_recv(bytes_view(bytes(raw.data(), raw.size())));
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        mock->connect_num_result = 0;
    });
    t.join();
    EXPECT_TRUE(called);
    EXPECT_EQ(act, action::GET);
}

TEST(gt_ahttp, server_parse_post_body) {
    std::atomic<bool> done{false};
    std::string received_body;
    std::thread t([&] {
        auto* mock = new MockTransmit();
        api* root = new api([&](const request& _req) -> reply {
            received_body = std::string((const char*) _req.get_content().data(),
                                        _req.get_content().size());
            done = true;
            return reply(version::V1_1, state::succ::OK, true);
        });
        http::server srv(mock, root, 1, 16);
        srv.open();

        std::string body = "hello world";
        std::string raw =
            "POST / HTTP/1.1\r\n"
            "Content-Length: " +
            std::to_string(body.size()) +
            "\r\n\r\n" +
            body;
        mock->inject_recv(bytes_view(bytes(raw.data(), raw.size())));
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        mock->connect_num_result = 0;
    });
    t.join();
    EXPECT_TRUE(done);
    EXPECT_EQ(received_body, "hello world");
}

TEST(gt_ahttp, server_garbage_data) {
    std::atomic<bool> called{false};
    std::thread t([&] {
        auto* mock = new MockTransmit();
        api* root = new api([&](const request&) -> reply {
            called = true;
            return reply(version::V1_1, state::succ::OK, true);
        });
        http::server srv(mock, root, 1, 16);
        srv.open();

        std::string garbage(256, '\0');
        for (size_t i = 0; i < garbage.size(); ++i) garbage[i] = (char) (i % 256);
        mock->inject_recv(bytes_view(bytes(garbage.data(), garbage.size())));
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        mock->connect_num_result = 0;
    });
    t.join();
    EXPECT_FALSE(called);
}

TEST(gt_ahttp, server_no_http_marker) {
    std::atomic<bool> called{false};
    std::thread t([&] {
        auto* mock = new MockTransmit();
        api* root = new api([&](const request&) -> reply {
            called = true;
            return reply(version::V1_1, state::succ::OK, true);
        });
        http::server srv(mock, root, 1, 16);
        srv.open();

        std::string raw = "HELLO / HTTP/9.9\r\n\r\n";
        mock->inject_recv(bytes_view(bytes(raw.data(), raw.size())));
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        mock->connect_num_result = 0;
    });
    t.join();
    EXPECT_FALSE(called);
}

TEST(gt_ahttp, server_truncated_header) {
    std::atomic<bool> called{false};
    std::thread t([&] {
        auto* mock = new MockTransmit();
        api* root = new api([&](const request&) -> reply {
            called = true;
            return reply(version::V1_1, state::succ::OK, true);
        });
        http::server srv(mock, root, 1, 16);
        srv.open();

        std::string raw = "GET / HTTP/1.1\r\nHost: test";
        mock->inject_recv(bytes_view(bytes(raw.data(), raw.size())));
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        mock->connect_num_result = 0;
    });
    t.join();
    EXPECT_FALSE(called);
}

TEST(gt_ahttp, server_no_method_line) {
    std::atomic<bool> called{false};
    std::thread t([&] {
        auto* mock = new MockTransmit();
        api* root = new api([&](const request&) -> reply {
            called = true;
            return reply(version::V1_1, state::succ::OK, true);
        });
        http::server srv(mock, root, 1, 16);
        srv.open();

        std::string raw = "HTTP/1.1 200 OK\r\n\r\n";
        mock->inject_recv(bytes_view(bytes(raw.data(), raw.size())));
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        mock->connect_num_result = 0;
    });
    t.join();
    EXPECT_FALSE(called);
}

TEST(gt_ahttp, server_leading_crlf_request) {
    std::atomic<bool> called{false};
    action act = action::NONE;
    std::thread t([&] {
        auto* mock = new MockTransmit();
        api* root = new api([&](const request& _req) -> reply {
            called = true;
            act = _req.get_action();
            return reply(version::V1_1, state::succ::OK, true);
        });
        http::server srv(mock, root, 1, 16);
        srv.open();

        std::string raw = "\r\n\r\nGET / HTTP/1.1\r\nHost: test\r\n\r\n";
        mock->inject_recv(bytes_view(bytes(raw.data(), raw.size())));
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        mock->connect_num_result = 0;
    });
    t.join();
    EXPECT_TRUE(called);
    EXPECT_EQ(act, action::GET);
}

TEST(gt_ahttp, server_bad_percent_escape) {
    std::atomic<int> served{0};
    std::thread t([&] {
        auto* mock = new MockTransmit();
        api* root = new api([&](const request&) -> reply {
            served++;
            return reply(version::V1_1, state::succ::OK, true);
        });
        http::server srv(mock, root, 1, 16);
        srv.open();

        std::string bad = "GET /%zz HTTP/1.1\r\nHost: test\r\n\r\n";
        mock->inject_recv(bytes_view(bytes(bad.data(), bad.size())));
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        std::string query = "GET /?a=%zz HTTP/1.1\r\nHost: test\r\n\r\n";
        mock->inject_recv(bytes_view(bytes(query.data(), query.size())));
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        mock->connect_num_result = 0;
    });
    t.join();
    EXPECT_EQ(served, 1);
}

TEST(gt_ahttp, server_declared_body_past_the_cap_is_dropped) {
    std::atomic<int> served{0};
    std::thread t([&] {
        auto* mock = new MockTransmit();
        api* root = new api([&](const request&) -> reply {
            served++;
            return reply(version::V1_1, state::succ::OK, true);
        });
        http::server srv(mock, root, 1, 16);
        srv.open();

        std::string raw = "POST / HTTP/1.1\r\nContent-Length: 9999999999\r\n\r\n";
        mock->inject_recv(bytes_view(bytes(raw.data(), raw.size())));
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        std::string ok = "GET /?a=b HTTP/1.1\r\nHost: test\r\n\r\n";
        mock->inject_recv(bytes_view(bytes(ok.data(), ok.size())));
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        mock->connect_num_result = 0;
    });
    t.join();
    EXPECT_EQ(served, 1);
}

TEST(gt_ahttp, client_parse_response) {
    auto* mock = new MockTransmit();
    http::client cli(mock);
    ASSERT_TRUE(cli.open());

    request req(action::GET, version::V1_1, true);
    req.set_urlwords({"/"}, {});
    std::future<reply> fut;
    ASSERT_TRUE(cli.exec(req, fut));
    ASSERT_TRUE(wait_for([&] { return mock->last_sent.size() > 0; }));

    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "hello";
    mock->inject_recv(bytes_view(bytes(raw.data(), raw.size())));

    ASSERT_EQ(fut.wait_for(std::chrono::seconds(3)), std::future_status::ready);
    reply r = fut.get();
    EXPECT_EQ(r.get_state(), state::succ::OK);
    EXPECT_EQ(r.get_content(), bytes("hello"));
    mock->connect_num_result = 0;
}

TEST(gt_ahttp, client_chunked) {
    auto* mock = new MockTransmit();
    http::client cli(mock);
    ASSERT_TRUE(cli.open());

    request req(action::GET, version::V1_1, true);
    req.set_urlwords({"/"}, {});
    std::future<reply> fut;
    ASSERT_TRUE(cli.exec(req, fut));
    ASSERT_TRUE(wait_for([&] { return mock->last_sent.size() > 0; }));

    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "5\r\n"
        "hello\r\n"
        "6\r\n"
        " world\r\n"
        "0\r\n"
        "\r\n";
    mock->inject_recv(bytes_view(bytes(raw.data(), raw.size())));

    ASSERT_EQ(fut.wait_for(std::chrono::seconds(3)), std::future_status::ready);
    reply r = fut.get();
    EXPECT_EQ(r.get_state(), state::succ::OK);
    EXPECT_EQ(r.get_content(), bytes("hello world"));
    mock->connect_num_result = 0;
}

TEST(gt_ahttp, client_garbage_response) {
    auto* mock = new MockTransmit();
    http::client cli(mock);
    ASSERT_TRUE(cli.open());

    request req(action::GET, version::V1_1, true);
    req.set_urlwords({"/"}, {});
    std::future<reply> fut;
    ASSERT_TRUE(cli.exec(req, fut));
    ASSERT_TRUE(wait_for([&] { return mock->last_sent.size() > 0; }));

    mock->inject_recv(bytes_view(bytes("garbage data here", 16)));

    EXPECT_NE(fut.wait_for(std::chrono::milliseconds(300)), std::future_status::ready);
    mock->connect_num_result = 0;
}

TEST(gt_ahttp, client_truncated_chunked) {
    auto* mock = new MockTransmit();
    http::client cli(mock);
    ASSERT_TRUE(cli.open());

    request req(action::GET, version::V1_1, true);
    req.set_urlwords({"/"}, {});
    std::future<reply> fut;
    ASSERT_TRUE(cli.exec(req, fut));
    ASSERT_TRUE(wait_for([&] { return mock->last_sent.size() > 0; }));

    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "5\r\n"
        "hello\r\n";

    mock->inject_recv(bytes_view(bytes(raw.data(), raw.size())));

    EXPECT_NE(fut.wait_for(std::chrono::milliseconds(300)), std::future_status::ready);
    mock->connect_num_result = 0;
}

TEST(gt_ahttp, client_malformed_status_line) {
    auto* mock = new MockTransmit();
    http::client cli(mock);
    ASSERT_TRUE(cli.open());

    request req(action::GET, version::V1_1, true);
    req.set_urlwords({"/"}, {});
    std::future<reply> fut;
    ASSERT_TRUE(cli.exec(req, fut));
    ASSERT_TRUE(wait_for([&] { return mock->last_sent.size() > 0; }));

    mock->inject_recv(bytes_view(bytes("HTTP/1.1 XX OK\r\n\r\n", 19)));

    EXPECT_NE(fut.wait_for(std::chrono::milliseconds(300)), std::future_status::ready);
    mock->connect_num_result = 0;
}

TEST(gt_ahttp, client_malformed_content_length) {
    auto* mock = new MockTransmit();
    http::client cli(mock);
    ASSERT_TRUE(cli.open());

    request req(action::GET, version::V1_1, true);
    req.set_urlwords({"/"}, {});
    std::future<reply> fut;
    ASSERT_TRUE(cli.exec(req, fut));
    ASSERT_TRUE(wait_for([&] { return mock->last_sent.size() > 0; }));

    std::string raw = "HTTP/1.1 200 OK\r\nContent-Length: abc\r\n\r\n";
    mock->inject_recv(bytes_view(bytes(raw.data(), raw.size())));
    EXPECT_NE(fut.wait_for(std::chrono::milliseconds(300)), std::future_status::ready);

    std::string raw2 = "HTTP/1.1 200 OK\r\nContent-Length: 99999999999999999999999\r\n\r\n";
    mock->inject_recv(bytes_view(bytes(raw2.data(), raw2.size())));
    EXPECT_NE(fut.wait_for(std::chrono::milliseconds(300)), std::future_status::ready);
    mock->connect_num_result = 0;
}

TEST(gt_ahttp, client_chunked_empty_body) {
    auto* mock = new MockTransmit();
    http::client cli(mock);
    ASSERT_TRUE(cli.open());

    request req(action::GET, version::V1_1, true);
    req.set_urlwords({"/"}, {});
    std::future<reply> fut;
    ASSERT_TRUE(cli.exec(req, fut));
    ASSERT_TRUE(wait_for([&] { return mock->last_sent.size() > 0; }));

    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "0\r\n"
        "\r\n";
    mock->inject_recv(bytes_view(bytes(raw.data(), raw.size())));

    ASSERT_EQ(fut.wait_for(std::chrono::seconds(3)), std::future_status::ready);
    reply r = fut.get();
    EXPECT_EQ(r.get_state(), state::succ::OK);
    EXPECT_TRUE(r.get_content().empty());
    mock->connect_num_result = 0;
}

TEST(gt_ahttp, client_chunked_binary_data) {
    auto* mock = new MockTransmit();
    http::client cli(mock);
    ASSERT_TRUE(cli.open());

    request req(action::GET, version::V1_1, true);
    req.set_urlwords({"/"}, {});
    std::future<reply> fut;
    ASSERT_TRUE(cli.exec(req, fut));
    ASSERT_TRUE(wait_for([&] { return mock->last_sent.size() > 0; }));

    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "5\r\n"
        "a\r\nbc\r\n"
        "0\r\n"
        "\r\n";
    mock->inject_recv(bytes_view(bytes(raw.data(), raw.size())));

    ASSERT_EQ(fut.wait_for(std::chrono::seconds(3)), std::future_status::ready);
    reply r = fut.get();
    EXPECT_EQ(r.get_state(), state::succ::OK);
    EXPECT_EQ(r.get_content(), bytes("a\r\nbc"));
    mock->connect_num_result = 0;
}

TEST(gt_ahttp, client_gzip_response) {
    auto* mock = new MockTransmit();
    http::client cli(mock);
    ASSERT_TRUE(cli.open());

    request req(action::GET, version::V1_1, true);
    req.set_urlwords({"/"}, {});
    std::future<reply> fut;
    ASSERT_TRUE(cli.exec(req, fut));
    ASSERT_TRUE(wait_for([&] { return mock->last_sent.size() > 0; }));

    EXPECT_TRUE(contains(mock->last_sent, "Accept-Encoding: gzip"));

    std::string body = "gzip compressed body test data";
    bytes compressed = compress::encoder_gzip::sexec(bytes(body.data(), body.size()));

    std::string raw =
        "HTTP/1.1 200 OK\r\n"
        "Content-Encoding: gzip\r\n"
        "Content-Length: " +
        std::to_string(compressed.size()) +
        "\r\n"
        "\r\n";
    bytes resp(raw.data(), raw.size());
    resp.append(compressed);
    mock->inject_recv(bytes_view(resp));

    ASSERT_EQ(fut.wait_for(std::chrono::seconds(3)), std::future_status::ready);
    reply r = fut.get();
    EXPECT_EQ(r.get_state(), state::succ::OK);

    EXPECT_EQ(r.get_content(), bytes(body.data(), body.size()));
    mock->connect_num_result = 0;
}

TEST(gt_ahttp, server_gzip_reply) {
    std::atomic<bool> called{false};
    bytes sent_to_client;
    std::thread t([&] {
        auto* mock = new MockTransmit();
        api* root = new api([&](const request&) -> reply {
            called = true;
            reply rpl(version::V1_1, state::succ::OK, true);
            rpl.set_content(mime::txt::plain, bytes_view(bytes(std::string(300, 'A'))));
            return rpl;
        });
        mock->on_send = [&](const bytes_view& _data, uint_64) {
            sent_to_client = _data.to_bytes();
        };
        http::server srv(mock, root, 1, 16);
        srv.open();

        std::string raw =
            "GET / HTTP/1.1\r\n"
            "Host: test\r\n"
            "Accept-Encoding: gzip\r\n"
            "\r\n";
        mock->inject_recv(bytes_view(bytes(raw.data(), raw.size())));
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        mock->connect_num_result = 0;
    });
    t.join();
    EXPECT_TRUE(called);
    EXPECT_TRUE(contains(sent_to_client, "Content-Encoding: gzip"));
}

TEST(gt_ahttp, client_disconnect_completes_pending_request) {
    auto* mock = new MockTransmit();
    http::client cli(mock);
    ASSERT_TRUE(cli.open());

    request req(action::GET, version::V1_1, true);
    req.set_urlwords({"/"}, {});
    std::future<reply> fut;
    ASSERT_TRUE(cli.exec(req, fut));

    mock->inject_connect(0, false, "close");

    ASSERT_EQ(fut.wait_for(std::chrono::seconds(3)), std::future_status::ready);
    EXPECT_EQ(fut.get().get_state(), state::NONE);

    std::future<reply> next;
    ASSERT_TRUE(cli.exec(req, next));
    const std::string raw = "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nhello";
    mock->inject_recv(bytes_view(bytes(raw.data(), raw.size())));
    ASSERT_EQ(next.wait_for(std::chrono::seconds(3)), std::future_status::ready);
    EXPECT_EQ(next.get().get_content(), bytes("hello"));
    mock->connect_num_result = 0;
}

TEST(gt_ahttp, client_interrupted_reply_is_not_reused) {
    auto* mock = new MockTransmit();
    http::client cli(mock);
    ASSERT_TRUE(cli.open());

    request req(action::GET, version::V1_1, true);
    req.set_urlwords({"/"}, {});
    std::future<reply> fut;
    ASSERT_TRUE(cli.exec(req, fut));

    const std::string half = "HTTP/1.1 200 OK\r\nContent-Length: 11\r\n\r\nhel";
    mock->inject_recv(bytes_view(bytes(half.data(), half.size())));
    mock->inject_connect(0, false, "close");

    ASSERT_EQ(fut.wait_for(std::chrono::seconds(3)), std::future_status::ready);
    EXPECT_EQ(fut.get().get_state(), state::NONE);

    std::future<reply> next;
    ASSERT_TRUE(cli.exec(req, next));
    const std::string raw = "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nworld";
    mock->inject_recv(bytes_view(bytes(raw.data(), raw.size())));
    ASSERT_EQ(next.wait_for(std::chrono::seconds(3)), std::future_status::ready);
    EXPECT_EQ(next.get().get_content(), bytes("world"));
    mock->connect_num_result = 0;
}

TEST(gt_ahttp, client_connect_event_keeps_pending_request) {
    auto* mock = new MockTransmit();
    http::client cli(mock);
    ASSERT_TRUE(cli.open());

    request req(action::GET, version::V1_1, true);
    req.set_urlwords({"/"}, {});
    std::future<reply> fut;
    ASSERT_TRUE(cli.exec(req, fut));

    mock->inject_connect(0, true, "open");
    EXPECT_EQ(fut.wait_for(std::chrono::milliseconds(100)), std::future_status::timeout);

    const std::string raw = "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nhello";
    mock->inject_recv(bytes_view(bytes(raw.data(), raw.size())));
    ASSERT_EQ(fut.wait_for(std::chrono::seconds(3)), std::future_status::ready);
    EXPECT_EQ(fut.get().get_content(), bytes("hello"));
    mock->connect_num_result = 0;
}

TEST(gt_ahttp, client_disconnect_drops_queued_send) {
    auto* mock = new MockTransmit();
    std::atomic<int> sends{0};
    mock->on_send = [&](const bytes_view&, uint_64) { ++sends; };
    mock->connect_num_result = 0;
    http::client cli(mock);
    ASSERT_TRUE(cli.open());

    ASSERT_TRUE(cli.bytes_send(bytes_view(bytes("stale", 5))));
    mock->inject_connect(0, false, "close");

    mock->connect_num_result = 1;
    ASSERT_TRUE(cli.bytes_send(bytes_view(bytes("fresh", 5))));
    ASSERT_TRUE(wait_for([&] { return sends.load() >= 1; }));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(sends.load(), 1) << "a queued send survived the disconnect";
    mock->connect_num_result = 0;
}

TEST(gt_ahttp, client_real_peer_close_completes_pending_request) {
    const std::string addr = "127.0.0.1:19646";
    std::unique_ptr<transmit> peer(transmit::create("tcp_server", addr));
    ASSERT_TRUE(peer != nullptr);
    ASSERT_TRUE(peer->open());

    http::client cli(addr);
    ASSERT_TRUE(cli.open());
    ASSERT_TRUE(wait_for([&] { return peer->connect_num() == 1; }));

    request req(action::GET, version::V1_1, true);
    req.set_urlwords({"/"}, {});
    std::future<reply> fut;
    ASSERT_TRUE(cli.exec(req, fut));

    peer->close();
    ASSERT_EQ(fut.wait_for(std::chrono::seconds(5)), std::future_status::ready);
    EXPECT_EQ(fut.get().get_state(), state::NONE);

    std::unique_ptr<transmit> back(transmit::create("tcp_server", addr));
    ASSERT_TRUE(back != nullptr);
    ASSERT_TRUE(back->open());
    ASSERT_TRUE(wait_for([&] { return back->connect_num() == 1; }));

    std::future<reply> next;
    ASSERT_TRUE(cli.exec(req, next));
    const std::string raw = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
    ASSERT_TRUE(back->bytes_send(raw.c_str(), raw.size()));
    ASSERT_EQ(next.wait_for(std::chrono::seconds(5)), std::future_status::ready);
    EXPECT_EQ(next.get().get_content(), bytes("ok"));
}
