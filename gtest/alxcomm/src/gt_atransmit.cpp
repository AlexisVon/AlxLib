/*****************************************************************/ /**
 * \file   gt_atransmit.cpp
 * \brief  Unit tests for transmit: address helpers, every transport type,
 *         and the TLS path (handshake, verification, mutual auth)
 *
 * \author alexis
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "atransmit.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <list>
#include <memory>
#include <string>
#include <thread>

using namespace alx;

TEST(gt_atransmit, from_strip_valid) {
    EXPECT_EQ(transmit::from_strip("192.168.1.1:8080"), std::make_pair(0XC0A80101U, (uint_16) 8080));
    EXPECT_EQ(transmit::from_strip("255.255.255.255:65535"), std::make_pair(0XFFFFFFFFU, (uint_16) 65535));
    EXPECT_EQ(transmit::from_strip("0.0.0.0:0"), std::make_pair(0U, (uint_16) 0));
}

TEST(gt_atransmit, from_strip_rejects_out_of_range) {

    EXPECT_EQ(transmit::from_strip("9999999999.1.1.1:80"), std::make_pair(0U, (uint_16) 0));
    EXPECT_EQ(transmit::from_strip("1.2.3.4:99999999999"), std::make_pair(0U, (uint_16) 0));
    EXPECT_EQ(transmit::from_strip("300.1.1.1:80"), std::make_pair(0U, (uint_16) 0));
    EXPECT_EQ(transmit::from_strip("1..2.3:80"), std::make_pair(0U, (uint_16) 0));
}

TEST(gt_atransmit, to_strip_round_trip) {
    for (const char* text : {"192.168.1.1:8080", "127.0.0.1:1", "255.255.255.255:65535"}) {
        const std::pair<uint_32, uint_16> parsed = transmit::from_strip(text);
        const uint_64 packed = ((uint_64) parsed.first << 32) | (uint_64) parsed.second;
        EXPECT_EQ(transmit::to_strip(packed), text);
    }
}

TEST(gt_atransmit, from_strip64_packs_ip_high_port_low) {
    const uint_64 packed = transmit::from_strip64("1.2.3.4:5678");
    EXPECT_EQ(packed >> 32, 0x01020304U);
    EXPECT_EQ((uint_16) (packed & 0xFFFF), 5678);
    EXPECT_EQ(transmit::from_strip64("garbage"), 0U);
}

TEST(gt_atransmit, get_addr_port_resolves_localhost) {
    const std::list<std::string> resolved = transmit::get_addr_port("localhost", "80");
    EXPECT_FALSE(resolved.empty());
    EXPECT_NE(std::find(resolved.begin(), resolved.end(), "127.0.0.1:80"), resolved.end());

    EXPECT_TRUE(transmit::get_addr_port("no.such.host.invalid.", "80").empty());
}

namespace {

constexpr uint_16 kTcpPort = 19641;
constexpr uint_16 kUdpPortA = 19642;
constexpr uint_16 kUdpPortB = 19643;
constexpr uint_16 kLocalPort = 19644;
constexpr uint_16 kTlsPort = 19645;

const char* const kTmpDir = "tmp-alxcomm-gtest";

const char* const kCertPem = R"PEM(-----BEGIN CERTIFICATE-----
MIIBjDCCATKgAwIBAgIBATAKBggqhkjOPQQDAjA3MQswCQYDVQQGEwJYWDEUMBIG
A1UECgwLQWx4TGliIFRlc3QxEjAQBgNVBAMMCWxvY2FsaG9zdDAeFw0yNjA5MTEx
MDU2MjNaFw0zNjA5MDgxMDU2MjNaMDcxCzAJBgNVBAYTAlhYMRQwEgYDVQQKDAtB
bHhMaWIgVGVzdDESMBAGA1UEAwwJbG9jYWxob3N0MFkwEwYHKoZIzj0CAQYIKoZI
zj0DAQcDQgAEUzI+k4tjpVKeKFJ6wTTGotBzFfaMqDEYupocYEGMTs6dAZbPFikx
WMsPyISgcTKgy8YGt3033ZAWwEYw4cdsUaMvMC0wGgYDVR0RBBMwEYIJbG9jYWxo
b3N0hwR/AAABMA8GA1UdEwEB/wQFMAMBAf8wCgYIKoZIzj0EAwIDSAAwRQIhAOvb
plXOnFpXA3ptGUl4TX/yiekw9s6MCAVqWvthaxTyAiBNcQt7GMm+/a/4hcJb4Hzs
bwejRXIczA3IodSci4+7Wg==
-----END CERTIFICATE-----
)PEM";
const char* const kKeyPem = R"PEM(-----BEGIN PRIVATE KEY-----
MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgTWuslQRYQMy7n+iX
OKy81WMn9n0nQcl5fDFZ1h0ut8mhRANCAARTMj6Ti2OlUp4oUnrBNMai0HMV9oyo
MRi6mhxgQYxOzp0Bls8WKTFYyw/IhKBxMqDLxga3fTfdkBbARjDhx2xR
-----END PRIVATE KEY-----
)PEM";

bool has_type(const char* _name) {
    const std::list<std::string> all = transmit::list_all();
    return std::find(all.begin(), all.end(), _name) != all.end();
}

std::string addr_of(uint_16 _port) {
    return "127.0.0.1:" + std::to_string((int) _port);
}

std::string tmp_path(const std::string& _name) {
    return (std::filesystem::path(kTmpDir) / _name).string();
}

std::string write_tmp(const std::string& _name, const char* _pem) {
    const std::string path = tmp_path(_name);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << _pem;
    out.close();
    return path;
}

bool wait_for(const std::function<bool()>& _cond, int _ms = 10000) {
    for (int i = 0; i < _ms / 20 && !_cond(); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    return _cond();
}

class capture {
public:
    void attach(transmit* _t) {
        _t->bytes_recv.connect([this](const bytes_view& _d, uint_64) {
            text.assign((const char*) _d.data(), (size_t) _d.size());
            ++hits;
            got.store(true);
        });
    }

    bool received(const std::string& _expect, int _ms = 10000) {
        return wait_for([&] { return got.load(); }, _ms) && text == _expect;
    }

    std::atomic<bool> got{false};
    std::atomic<int> hits{0};
    std::string text;
};

class error_log {
public:
    void attach(transmit* _t) {
        _t->trans_mesg.connect([this](uint_64, bool _ok, const char* _msg) {
            if (!_ok && _msg != nullptr) message = _msg;
        });
    }
    std::string message;
};

}

TEST(gt_atransmit, list_all_registers_the_documented_transports) {
    for (const char* name : {"tcp_server", "tcp_client", "udp_trans", "local_server", "local_client"})
        EXPECT_TRUE(has_type(name)) << name << " is not registered";

    EXPECT_EQ(has_type("tls_server"), has_type("tls_client"));
}

TEST(gt_atransmit, create_unknown_type_returns_null) {
    std::unique_ptr<transmit> bad(transmit::create("no_such_transport", addr_of(kTcpPort)));
    EXPECT_EQ(bad, nullptr);
}

TEST(gt_atransmit, tcp_round_trip_both_directions) {
    capture srv_cap, cli_cap;
    std::unique_ptr<transmit> srv(transmit::create("tcp_server", addr_of(kTcpPort)));
    ASSERT_TRUE(srv != nullptr);
    srv_cap.attach(srv.get());
    ASSERT_TRUE(srv->open());

    std::unique_ptr<transmit> cli(transmit::create("tcp_client", addr_of(kTcpPort)));
    ASSERT_TRUE(cli != nullptr);
    cli_cap.attach(cli.get());
    ASSERT_TRUE(cli->open());

    ASSERT_TRUE(wait_for([&] { return srv->connect_num() == 1; })) << "server never accepted the client";
    EXPECT_TRUE(cli->is_open());

    ASSERT_TRUE(cli->bytes_send("ping", 4));
    EXPECT_TRUE(srv_cap.received("ping")) << "server got: " << srv_cap.text;

    ASSERT_TRUE(srv->bytes_send("pong", 4));
    EXPECT_TRUE(cli_cap.received("pong")) << "client got: " << cli_cap.text;

    cli->close();
    EXPECT_TRUE(wait_for([&] { return srv->connect_num() == 0; })) << "server still reports a peer after close";
}

TEST(gt_atransmit, tcp_server_close_loc_reports_disconnect) {
    std::unique_ptr<transmit> srv(transmit::create("tcp_server", addr_of(kTcpPort)));
    ASSERT_TRUE(srv != nullptr);
    std::atomic<uint_64> peer_key{0};
    std::atomic<bool> disconnected{false};
    srv->trans_mesg.connect([&](uint_64 _loc, bool _ok, const char*) {
        if (0 == _loc) return;
        if (_ok) peer_key.store(_loc);
        else disconnected.store(true);
    });
    ASSERT_TRUE(srv->open());

    std::unique_ptr<transmit> cli(transmit::create("tcp_client", addr_of(kTcpPort)));
    ASSERT_TRUE(cli != nullptr);
    ASSERT_TRUE(cli->open());
    ASSERT_TRUE(wait_for([&] { return srv->connect_num() == 1; }));
    ASSERT_TRUE(wait_for([&] { return peer_key.load() != 0; }));

    srv->close(peer_key.load());
    EXPECT_TRUE(wait_for([&] { return disconnected.load(); })) << "close(loc) left the peer's disconnect unreported";
    EXPECT_EQ(srv->connect_num(), 0);
}

TEST(gt_atransmit, tcp_client_send_before_open_fails) {
    std::unique_ptr<transmit> cli(transmit::create("tcp_client", addr_of(kTcpPort)));
    ASSERT_TRUE(cli != nullptr);
    EXPECT_FALSE(cli->is_open());
    EXPECT_FALSE(cli->bytes_send("x", 1));
}

TEST(gt_atransmit, udp_round_trip_both_directions) {
    capture cap_a, cap_b;
    std::unique_ptr<transmit> a(transmit::create("udp_trans", addr_of(kUdpPortA)));
    std::unique_ptr<transmit> b(transmit::create("udp_trans", addr_of(kUdpPortB)));
    ASSERT_TRUE(a != nullptr);
    ASSERT_TRUE(b != nullptr);
    cap_a.attach(a.get());
    cap_b.attach(b.get());

    ASSERT_TRUE(a->open());
    ASSERT_TRUE(b->open());

    ASSERT_TRUE(a->bytes_send("udp-hello", 9, transmit::from_strip64(addr_of(kUdpPortB))));
    EXPECT_TRUE(cap_b.received("udp-hello")) << "b got: " << cap_b.text;

    ASSERT_TRUE(b->bytes_send("udp-world", 9, transmit::from_strip64(addr_of(kUdpPortA))));
    EXPECT_TRUE(cap_a.received("udp-world")) << "a got: " << cap_a.text;
}

TEST(gt_atransmit, local_round_trip_both_directions) {
    capture srv_cap, cli_cap;
    error_log srv_err;

    std::unique_ptr<transmit> srv(transmit::create("local_server", addr_of(kLocalPort)));
    ASSERT_TRUE(srv != nullptr);
    srv_cap.attach(srv.get());
    srv_err.attach(srv.get());

    if (!srv->open())
        GTEST_SKIP() << "AF_UNIX not usable on this host: " << srv_err.message;

    std::unique_ptr<transmit> cli(transmit::create("local_client", addr_of(kLocalPort)));
    ASSERT_TRUE(cli != nullptr);
    cli_cap.attach(cli.get());
    ASSERT_TRUE(cli->open());

    ASSERT_TRUE(wait_for([&] { return srv->connect_num() == 1; })) << "server never accepted the client";

    ASSERT_TRUE(cli->bytes_send("local-ping", 10));
    EXPECT_TRUE(srv_cap.received("local-ping")) << "server got: " << srv_cap.text;

    ASSERT_TRUE(srv->bytes_send("local-pong", 10));
    EXPECT_TRUE(cli_cap.received("local-pong")) << "client got: " << cli_cap.text;
}

TEST(gt_atransmit, local_server_close_loc_reports_disconnect) {
    error_log srv_err;
    std::unique_ptr<transmit> srv(transmit::create("local_server", addr_of(kLocalPort)));
    ASSERT_TRUE(srv != nullptr);
    srv_err.attach(srv.get());
    std::atomic<uint_64> peer_key{0};
    std::atomic<bool> disconnected{false};
    srv->trans_mesg.connect([&](uint_64 _loc, bool _ok, const char*) {
        if (0 == _loc) return;
        if (_ok) peer_key.store(_loc);
        else disconnected.store(true);
    });

    if (!srv->open())
        GTEST_SKIP() << "AF_UNIX not usable on this host: " << srv_err.message;

    std::unique_ptr<transmit> cli(transmit::create("local_client", addr_of(kLocalPort)));
    ASSERT_TRUE(cli != nullptr);
    ASSERT_TRUE(cli->open());
    ASSERT_TRUE(wait_for([&] { return srv->connect_num() == 1; }));
    ASSERT_TRUE(wait_for([&] { return peer_key.load() != 0; }));

    srv->close(peer_key.load());
    EXPECT_TRUE(wait_for([&] { return disconnected.load(); })) << "close(loc) left the peer's disconnect unreported";
    EXPECT_EQ(srv->connect_num(), 0);
}

namespace {

bool tls_available() {
    return has_type("tls_server") && has_type("tls_client");
}

}

TEST(gt_atransmit, tls_round_trip_both_directions) {
    if (!tls_available())
        GTEST_SKIP() << "TLS support is not compiled into this build";

    const std::string cert = write_tmp("server_cert.pem", kCertPem);
    const std::string key = write_tmp("server_key.pem", kKeyPem);

    varmap srv_ext;
    srv_ext.insert("cert", cert);
    srv_ext.insert("key", key);

    varmap cli_ext;
    cli_ext.insert("ca", cert);
    cli_ext.insert("hostname", std::string("localhost"));

    capture srv_cap, cli_cap;
    std::unique_ptr<transmit> srv(transmit::create("tls_server", addr_of(kTlsPort), srv_ext));
    std::unique_ptr<transmit> cli(transmit::create("tls_client", addr_of(kTlsPort), cli_ext));
    ASSERT_TRUE(srv != nullptr);
    ASSERT_TRUE(cli != nullptr);
    srv_cap.attach(srv.get());
    cli_cap.attach(cli.get());

    ASSERT_TRUE(srv->open()) << "tls_server failed to bind";

    ASSERT_TRUE(cli->open()) << "tls_client failed to connect/handshake";

    ASSERT_TRUE(cli->bytes_send("ping-over-tls", 13));
    EXPECT_TRUE(srv_cap.received("ping-over-tls")) << "server got: " << srv_cap.text;

    ASSERT_TRUE(srv->bytes_send("pong-over-tls", 13));
    EXPECT_TRUE(cli_cap.received("pong-over-tls")) << "client got: " << cli_cap.text;

    cli->close();
    srv->close();
}

TEST(gt_atransmit, tls_mutual_auth_round_trip) {
    if (!tls_available())
        GTEST_SKIP() << "TLS support is not compiled into this build";

    const std::string cert = write_tmp("mtls_cert.pem", kCertPem);
    const std::string key = write_tmp("mtls_key.pem", kKeyPem);

    varmap srv_ext;
    srv_ext.insert("cert", cert);
    srv_ext.insert("key", key);
    srv_ext.insert("ca", cert);

    varmap cli_ext;
    cli_ext.insert("ca", cert);
    cli_ext.insert("hostname", std::string("localhost"));
    cli_ext.insert("cert", cert);
    cli_ext.insert("key", key);

    capture srv_cap;
    std::unique_ptr<transmit> srv(transmit::create("tls_server", addr_of(kTlsPort), srv_ext));
    std::unique_ptr<transmit> cli(transmit::create("tls_client", addr_of(kTlsPort), cli_ext));
    ASSERT_TRUE(srv != nullptr);
    ASSERT_TRUE(cli != nullptr);
    srv_cap.attach(srv.get());

    ASSERT_TRUE(srv->open());
    ASSERT_TRUE(cli->open()) << "mutual-auth handshake failed";

    ASSERT_TRUE(cli->bytes_send("mtls-ping", 9));
    EXPECT_TRUE(srv_cap.received("mtls-ping")) << "server got: " << srv_cap.text;

    cli->close();
    srv->close();
}

TEST(gt_atransmit, tls_server_rejects_client_without_certificate) {
    if (!tls_available())
        GTEST_SKIP() << "TLS support is not compiled into this build";

    const std::string cert = write_tmp("nocert_cert.pem", kCertPem);
    const std::string key = write_tmp("nocert_key.pem", kKeyPem);

    varmap srv_ext;
    srv_ext.insert("cert", cert);
    srv_ext.insert("key", key);
    srv_ext.insert("ca", cert);

    varmap cli_ext;
    cli_ext.insert("ca", cert);
    cli_ext.insert("hostname", std::string("localhost"));

    capture srv_cap;
    std::unique_ptr<transmit> srv(transmit::create("tls_server", addr_of(kTlsPort), srv_ext));
    std::unique_ptr<transmit> cli(transmit::create("tls_client", addr_of(kTlsPort), cli_ext));
    ASSERT_TRUE(srv != nullptr);
    ASSERT_TRUE(cli != nullptr);
    srv_cap.attach(srv.get());

    ASSERT_TRUE(srv->open());

    if (cli->open())
        cli->bytes_send("should-not-arrive", 16);

    EXPECT_FALSE(srv_cap.received("should-not-arrive", 1500))
        << "server accepted a client that presented no certificate";

    cli->close();
    srv->close();
}

TEST(gt_atransmit, tls_client_requires_hostname_with_ca) {
    if (!tls_available())
        GTEST_SKIP() << "TLS support is not compiled into this build";

    varmap cli_ext;
    cli_ext.insert("ca", write_tmp("hostname_cert.pem", kCertPem));

    std::unique_ptr<transmit> cli(transmit::create("tls_client", addr_of(kTlsPort), cli_ext));
    ASSERT_TRUE(cli != nullptr);
    EXPECT_FALSE(cli->open()) << "ca without hostname must not be accepted";
}
