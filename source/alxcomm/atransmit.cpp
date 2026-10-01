/*****************************************************************/ /**
 * \file   atransmit.cpp
 * \brief  Data transmission layer (TCP client/server, UDP, local Unix domain socket, TLS)
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "atransmit.h"

#ifdef _WIN32
#    include <WS2tcpip.h>
#    include <WinSock2.h>
#    include <afunix.h>
#    pragma comment(lib, "ws2_32.lib")
typedef SOCKET socket_t;
typedef int SOCKET_ADDR_LEN;
typedef SOCKADDR_UN sockaddr_un;
#    define ALEXIS_SOCKET_VALUE INVALID_SOCKET
#    define ALEXIS_SOCKET_ERROR SOCKET_ERROR
#    define S_ADDR S_un.S_addr
#    ifndef UNIX_PATH_MAX
#        define sun_family Family
#        define sun_path Path
#    endif
#else
#    include <arpa/inet.h>
#    include <netdb.h>
#    include <netinet/in.h>
#    include <poll.h>
#    include <sys/socket.h>
#    include <sys/time.h>
#    include <sys/un.h>
typedef int socket_t;
typedef socklen_t SOCKET_ADDR_LEN;
#    define ALEXIS_SOCKET_VALUE -1
#    define ALEXIS_SOCKET_ERROR -1
#    define S_ADDR s_addr
#endif

#include "afactory.h"
#include "aplatform.h"
#include "astring.h"

#include <chrono>
#include <memory>

using namespace alx;

typedef factory<transmit, const std::string&, const varmap&> transmit_factory;
template <typename T>
using transmit_product = product<T, transmit, const std::string&, const varmap&>;

#if CPP_VERSION >= CPP_17_ID
using SHARED_MUTEX = std::shared_mutex;
using SHARED_LOCK = std::shared_lock<std::shared_mutex>;
using UNIQUE_LOCK = std::unique_lock<std::shared_mutex>;
#else
using SHARED_MUTEX = std::mutex;
using SHARED_LOCK = std::lock_guard<std::mutex>;
using UNIQUE_LOCK = std::lock_guard<std::mutex>;
#endif
// guards the peer tables only; serializing open/close and bytes_send is the caller's convention, not enforced
#define _READ__LOCK_(_MUTEX_)     \
    SHARED_LOCK _r_lock(_MUTEX_); \
    (void) _r_lock
#define _WRITE_LOCK_(_MUTEX_)     \
    UNIQUE_LOCK _w_lock(_MUTEX_); \
    (void) _w_lock

// the flag is set by the worker itself as its very last act, so a reaper can join only what is finished
using worker_slot = std::pair<std::shared_ptr<std::atomic<bool>>, std::thread>;

inline void join_threads(std::list<worker_slot>& _workers) {
    for (auto& worker : _workers)
        if (worker.second.joinable()) worker.second.join();
}

// the caller wakes or deletes the socket first: the receiver sits in recv and the join would hang
inline void release_receiver(std::thread*& _thread) {
    std::thread* thread = _thread;
    _thread = nullptr;
    if (nullptr != thread) {
        if (thread->joinable()) thread->join();
        safe_delete(thread);
    }
}

namespace wsa_utils {
    constexpr uint_64 min_recv_buffer_size{alx::bit_align_v<1, 12>};
    constexpr uint_64 max_recv_buffer_size{alx::bit_align_v<1, 22>};

    // SO_SNDTIMEO bound: a peer that stops draining must not hold a transmit thread forever
    constexpr int send_timeout_ms{5000};
    constexpr int handshake_timeout_ms{10000};
#ifdef _WIN32
    WSADATA wsa_data;
    static int init_wsa() {
        return WSAStartup(MAKEWORD(2, 2), &wsa_data);
    }
    static void uninit_wsa() {
        WSACleanup();
    }
#else
    static int init_wsa() { return 0; }
    static void uninit_wsa() {}
#endif
    static uint_64 sockaddr_2_uint64(const sockaddr_in& _addr) {
        uint_32 addr = ntohl(_addr.sin_addr.S_ADDR);
        uint_16 port = ntohs(_addr.sin_port);
        return ((uint_64) addr << 32) | (uint_64) port;
    }
    static sockaddr_in uint64_2_sockaddr(const uint_64 _addr) {
        sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_addr.S_ADDR = htonl((uint_32) (_addr >> 32));
        addr.sin_port = htons((uint_16) (_addr & 0xFFFF));
        return addr;
    }

    static inline bool io_interrupted(const int _ret) {
        if (ALEXIS_SOCKET_ERROR != _ret) return false;
#ifdef _WIN32
        return WSAEINTR == WSAGetLastError();
#else
        return EINTR == errno;
#endif
    }

    static bool send_data(socket_t _socket, const void* _data, const uint_64 _size) {
        uint_64 offset{0};
        while (_size > offset) {
            int send_cnt = send(_socket, (const char*) _data + offset, (int) (_size - offset),
#ifdef _WIN32
                                0
#else
                                MSG_NOSIGNAL
#endif
            );
            if (io_interrupted(send_cnt)) continue;
            if (send_cnt <= 0) return false;
            offset += send_cnt;
        }
        return true;
    }
    static bool send_data_to(socket_t _socket, const void* _data, const uint_64 _size, uint_64 _addr) {
        uint_64 offset{0};
        sockaddr_in addr = uint64_2_sockaddr(_addr);
        while (_size > offset) {
            int send_cnt = sendto(_socket, (const char*) _data + offset, (int) (_size - offset),
#ifdef _WIN32
                                  0,
#else
                                  MSG_NOSIGNAL,
#endif
                                  (sockaddr*) &addr, sizeof(addr));
            if (io_interrupted(send_cnt)) continue;
            if (send_cnt <= 0) return false;
            offset += send_cnt;
        }
        return true;
    }

    // wakes whoever is blocked on the socket without freeing the descriptor -- the owner closes it
    static void shutdown_socket(socket_t _socket) {
        if (_socket == ALEXIS_SOCKET_VALUE) return;
#ifdef _WIN32
        ::shutdown(_socket, SD_BOTH);
#else
        ::shutdown(_socket, SHUT_RDWR);
#endif
    }
    static void delete_socket(socket_t _socket) {
        if (_socket == ALEXIS_SOCKET_VALUE) return;
        shutdown_socket(_socket);
#ifdef _WIN32
        ::closesocket(_socket);
#else
        ::close(_socket);
#endif
    }
    static void set_send_timeout(socket_t _socket, int _ms = send_timeout_ms) {
        if (_socket == ALEXIS_SOCKET_VALUE) return;
#ifdef _WIN32
        DWORD tv = (DWORD) _ms;
        setsockopt(_socket, SOL_SOCKET, SO_SNDTIMEO, (const char*) &tv, sizeof(tv));
#else
        timeval tv{_ms / 1000, (_ms % 1000) * 1000};
        setsockopt(_socket, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
    }
    static void set_recv_timeout(socket_t _socket, int _ms) {
        if (_socket == ALEXIS_SOCKET_VALUE) return;
#ifdef _WIN32
        DWORD tv = (DWORD) _ms;
        setsockopt(_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*) &tv, sizeof(tv));
#else
        timeval tv{_ms / 1000, (_ms % 1000) * 1000};
        setsockopt(_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
    }
    static void adjust_buffer(bytes& _buffer, size_t _recv_size) {
        // grow only when a read filled the buffer; 1.5x, clamped to [4 KiB, 4 MiB]
        if (_recv_size < _buffer.size()) return;
        uint_64 new_size = _buffer.size() + (_buffer.size() >> 1);
        new_size = alx::border(new_size, min_recv_buffer_size, max_recv_buffer_size);
        return new_size != _buffer.size() ? _buffer.resize(new_size) : void();
    }
};

transmit* alx::transmit::create(const std::string& _type, const std::string& _addr_port, const varmap& _ext_info) {
    return transmit_factory::create(_type, _addr_port, _ext_info);
}

std::list<std::string> alx::transmit::list_all() {
    std::list<std::string> result;
    for (const auto& t : transmit_factory::type_map())
        result.push_back(t.first);
    return result;
}

std::string alx::transmit::to_strip(uint_64 _addr_port) {
    uint_8* addr = reinterpret_cast<uint_8*>(&_addr_port);
    return std::to_string((int) addr[7]) + '.' + std::to_string((int) addr[6]) + '.' + std::to_string((int) addr[5]) + '.' + std::to_string((int) addr[4]) + ':' + std::to_string(r_interpret<uint_16>(addr));
}

static bool strip_part(const std::string& _text, const uint_32 _max, uint_32& _value) {
    if (_text.empty()) return false;
    uint_32 value{0};
    for (const char ch : _text) {
        value = value * 10 + (uint_32) (ch - '0');
        if (value > _max) return false;
    }
    return _value = value, true;
}

std::pair<uint_32, uint_16> alx::transmit::from_strip(const std::string& _addr_port) {
    std::pair<uint_32, uint_16> result{0, 0};

    if (_addr_port.length() < 9 || _addr_port.length() > 21) return result;
    for (char ch : _addr_port)
        if (!isdigit((unsigned char) ch) && ch != '.' && ch != ':') return result;

    std::list<std::string> temp1 = strutil::split(_addr_port, ".");
    if (temp1.size() != 4) return result;
    std::list<std::string> temp2 = strutil::split(temp1.back(), ":");
    if (temp2.size() != 2) return result;
    temp1.pop_back();
    temp1.push_back(temp2.front());
    temp2.pop_front();

    uint_32 value{0}, ip{0};
    for (const std::string& _key : temp1) {
        if (!strip_part(_key, 0XFFU, value)) return result;
        ip = (ip << 8) | value;
    }
    if (!strip_part(temp2.front(), 0XFFFFU, value)) return result;
    result.first = ip;
    result.second = (uint_16) value;
    return result;
}

uint_64 alx::transmit::from_strip64(const std::string& _addr_port) {
    const std::pair<uint_32, uint_16> ip = from_strip(_addr_port);
    return ((uint_64) ip.first << 32) | (uint_64) ip.second;
}

std::list<std::string> alx::transmit::get_addr_port(const std::string& _node, const std::string& _service) {
    wsa_utils::init_wsa();
    addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(_node.c_str(), _service.c_str(), &hints, &res) != 0) {
        return {};
    }
    std::list<std::string> result;
    for (addrinfo* i = res; nullptr != i; i = i->ai_next) {
        sockaddr_in* addr = (sockaddr_in*) i->ai_addr;
        result.push_back(transmit::to_strip(
            (uint_64) ntohl(addr->sin_addr.s_addr) << 32 | ntohs(addr->sin_port)));
    }
    freeaddrinfo(res);
    wsa_utils::uninit_wsa();
    return result;
}

class tcp_server
    : public transmit_product<tcp_server> {
public:
    tcp_server(const std::string& _addr_port, const varmap&)
        : transmit_product<tcp_server>(_addr_port, varmap()) {
        if (0 != wsa_utils::init_wsa()) {
            trans_mesg.exec(0, false, "wsa start up error");
        }
        const std::pair<uint_32, uint_16> ip = from_strip(_addr_port);
        m_server_addr.sin_family = AF_INET;
        m_server_addr.sin_addr.S_ADDR = htonl(ip.first);
        m_server_addr.sin_port = htons(ip.second);
    }
    ~tcp_server() {
        close();
        wsa_utils::uninit_wsa();
    }

public:
    virtual bool open() override {
        if (m_server.load() != ALEXIS_SOCKET_VALUE) return true;
        socket_t server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (server == ALEXIS_SOCKET_VALUE) {
            trans_mesg.exec(0, false, "failed to create server socket");
            return false;
        }
        {
            int opt = 1;
            setsockopt(server, SOL_SOCKET, SO_REUSEADDR,
#ifdef _WIN32
                       (const char*) &opt, sizeof(opt));
#else
                       &opt, sizeof(opt));
#endif
        }
        if (bind(server, (sockaddr*) &m_server_addr, sizeof(m_server_addr)) == ALEXIS_SOCKET_ERROR ||
            listen(server, SOMAXCONN) == ALEXIS_SOCKET_ERROR) {
            trans_mesg.exec(0, false, "bind or listen port failed");
            wsa_utils::delete_socket(server);
            m_server.store(ALEXIS_SOCKET_VALUE);
            return false;
        }
        m_server.store(server);
        trans_mesg.exec(0, true, "open");
        m_thread = new std::thread(std::bind(&tcp_server::connect_server, this));
        return true;
    }
    virtual void close(uint_64 _loc = 0) override {
        if (m_server.load() != ALEXIS_SOCKET_VALUE) {
            if (0 == _loc) {
                wsa_utils::delete_socket(m_server.load());
                m_server.store(ALEXIS_SOCKET_VALUE);
                wake_workers();
                if (nullptr != m_thread && m_thread->joinable()) {
                    m_thread->join();
                    safe_delete(m_thread);
                }
                // second pass: the accept thread may have admitted one more connection before it stopped
                wake_workers();
                join_workers();
                trans_mesg.exec(0, false, "close");
            } else {
                _WRITE_LOCK_(m_mutex);
                auto iter = m_clients.find(_loc);

                if (iter != m_clients.end()) wsa_utils::shutdown_socket(iter->second);
            }
        }
    }
    virtual bool is_open() override { return m_server.load() != ALEXIS_SOCKET_VALUE; }
    virtual uint_64 connect_num() override {
        _READ__LOCK_(m_mutex);
        return m_clients.size();
    }
    virtual bool bytes_send(const void* _data, const uint_64 _size, uint_64 _loc) override {
        if (nullptr == _data || 0 == _size || connect_num() == 0) return true;
        uint_64 success_count{0};
        std::list<uint_64> failed_socket;
        {
            _READ__LOCK_(m_mutex);
            for (auto& client : m_clients)
                if (0 == _loc || _loc == client.first) {
                    if (wsa_utils::send_data(client.second, _data, _size)) success_count++;
                    else failed_socket.push_back(client.first);
                }
        }
        if (!failed_socket.empty())
            for (auto key : failed_socket) close(key);
        return 0 != success_count;
    }

private:
    void connect_server() {
        while (m_server.load() != ALEXIS_SOCKET_VALUE) {
            sockaddr_in client_addr;
            SOCKET_ADDR_LEN addr_len = sizeof(sockaddr_in);
            socket_t client = accept(m_server.load(), (sockaddr*) &client_addr, &addr_len);
            if (ALEXIS_SOCKET_VALUE == client) return;
            uint_32 addr = ntohl(client_addr.sin_addr.S_ADDR);
            uint_16 port = ntohs(client_addr.sin_port);
            const uint_64 key = ((uint_64) addr << 32) | (uint_64) port;
            wsa_utils::set_send_timeout(client);
            {
                _WRITE_LOCK_(m_mutex);
                m_clients[key] = client;
            }
            trans_mesg.exec(key, true, "connect");
            std::shared_ptr<std::atomic<bool>> done = std::make_shared<std::atomic<bool>>(false);
            std::thread worker(std::bind(&tcp_server::receive_server, this, client, key, done));
            {
                _WRITE_LOCK_(m_mutex);
                m_workers.emplace_back(done, std::move(worker));
            }
            reap_workers();
        }
    }
    void receive_server(socket_t _client, uint_64 _key, std::shared_ptr<std::atomic<bool>> _done) {
        bytes buffer(wsa_utils::min_recv_buffer_size);
        while (true) {
            int recv_size = recv(_client, (char*) buffer.data(), (int) buffer.size(), 0);
            if (wsa_utils::io_interrupted(recv_size)) continue;
            if (recv_size <= 0) {
                _WRITE_LOCK_(m_mutex);
                m_clients.erase(_key);
                wsa_utils::delete_socket(_client);
                break;
            }
            bytes_recv.exec(buffer.left_view(recv_size), _key);
            wsa_utils::adjust_buffer(buffer, recv_size);
        }
        trans_mesg.exec(_key, false, "disconnect");
        _done->store(true);
    }

private:

    void wake_workers() {
        _READ__LOCK_(m_mutex);
        for (auto& client : m_clients) wsa_utils::shutdown_socket(client.second);
    }
    void join_workers() {
        std::list<worker_slot> workers;
        {
            _WRITE_LOCK_(m_mutex);
            workers.splice(workers.end(), m_workers);
        }
        join_threads(workers);
    }

    void reap_workers() {
        std::list<worker_slot> workers;
        {
            _WRITE_LOCK_(m_mutex);
            for (auto iter = m_workers.begin(); iter != m_workers.end();) {
                if (iter->first->load()) workers.splice(workers.end(), m_workers, iter++);
                else ++iter;
            }
        }
        join_threads(workers);
    }

private:
    std::atomic<socket_t> m_server{ALEXIS_SOCKET_VALUE};
    std::unordered_map<uint_64, socket_t> m_clients;
    std::list<worker_slot> m_workers;
    SHARED_MUTEX m_mutex;
    std::thread* m_thread{nullptr};
    sockaddr_in m_server_addr;
};

class tcp_client
    : public transmit_product<tcp_client> {
public:
    tcp_client(const std::string& _addr_port, const varmap&)
        : transmit_product<tcp_client>(_addr_port, varmap()) {
        if (0 != wsa_utils::init_wsa()) {
            trans_mesg.exec(0, false, "wsa start up error");
        }
        const std::pair<uint_32, uint_16> ip = from_strip(_addr_port);
        m_server_addr.sin_family = AF_INET;
        m_server_addr.sin_addr.S_ADDR = htonl(ip.first);
        m_server_addr.sin_port = htons(ip.second);
    }
    ~tcp_client() {
        close();
        wsa_utils::uninit_wsa();
    }

public:
    virtual bool open() override {
        if (m_client.load() != ALEXIS_SOCKET_VALUE) return true;
        release_receiver(m_thread);

        socket_t client = ALEXIS_SOCKET_VALUE;
        if ((client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == ALEXIS_SOCKET_VALUE) {
            trans_mesg.exec(0, false, "failed to create client socket");
            return false;
        }
        if (connect(client, (sockaddr*) &m_server_addr, sizeof(m_server_addr)) == ALEXIS_SOCKET_ERROR) {
            wsa_utils::delete_socket(client);
            trans_mesg.exec(0, false, "connection failed");
            return false;
        }
        wsa_utils::set_send_timeout(client);
        m_client.store(client);
        trans_mesg.exec(0, true, "open");
        m_thread = new std::thread(std::bind(&tcp_client::receive_client, this));
        return true;
    }
    virtual void close(uint_64 _loc = 0) override {
        socket_t sock = m_client.exchange(ALEXIS_SOCKET_VALUE);
        if (sock != ALEXIS_SOCKET_VALUE) wsa_utils::delete_socket(sock);
        release_receiver(m_thread);
        if (sock != ALEXIS_SOCKET_VALUE) trans_mesg.exec(0, false, "close");
    }
    virtual bool is_open() override { return m_client.load() != ALEXIS_SOCKET_VALUE; }
    virtual uint_64 connect_num() override { return m_client.load() != ALEXIS_SOCKET_VALUE ? 1 : 0; }
    virtual bool bytes_send(const void* _data, const uint_64 _size, uint_64 _loc) override {
        socket_t sock = m_client.load();
        if (sock == ALEXIS_SOCKET_VALUE) return false;
        if (nullptr == _data || 0 == _size) return true;
        return wsa_utils::send_data(sock, _data, _size);
    }

private:
    void receive_client() {
        bytes buffer(wsa_utils::min_recv_buffer_size);
        while (true) {
            socket_t sock = m_client.load();
            if (sock == ALEXIS_SOCKET_VALUE) break;
            int recv_size = recv(sock, (char*) buffer.data(), (int) buffer.size(), 0);
            if (wsa_utils::io_interrupted(recv_size)) continue;
            if (recv_size <= 0) break;
            bytes_recv.exec(buffer.left_view(recv_size), 0);
            wsa_utils::adjust_buffer(buffer, recv_size);
        }
        socket_t sock = m_client.exchange(ALEXIS_SOCKET_VALUE);
        if (sock != ALEXIS_SOCKET_VALUE) {
            wsa_utils::delete_socket(sock);
            trans_mesg.exec(0, false, "close");
        }
    }

private:
    std::atomic<socket_t> m_client{ALEXIS_SOCKET_VALUE};
    std::thread* m_thread{nullptr};
    sockaddr_in m_server_addr;
};

class udp_trans
    : public transmit_product<udp_trans> {
public:
    udp_trans(const std::string& _addr_port, const varmap&)
        : transmit_product<udp_trans>(_addr_port, varmap()) {
        if (0 != wsa_utils::init_wsa()) {
            trans_mesg.exec(0, false, "wsa start up error");
        }
        const std::pair<uint_32, uint_16> ip = from_strip(_addr_port);
        m_local_addr.sin_family = AF_INET;
        m_local_addr.sin_addr.S_ADDR = htonl(ip.first);
        m_local_addr.sin_port = htons(ip.second);
    }
    ~udp_trans() {
        close();
        wsa_utils::uninit_wsa();
    }

public:
    virtual bool open() override {
        if (m_socket.load() != ALEXIS_SOCKET_VALUE) return true;

        socket_t local = ALEXIS_SOCKET_VALUE;
        if ((local = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == ALEXIS_SOCKET_VALUE) {
            trans_mesg.exec(0, false, "failed to create client socket");
            return false;
        }
        {
            int opt = 1;
            setsockopt(local, SOL_SOCKET, SO_REUSEADDR,
#ifdef _WIN32
                       (const char*) &opt, sizeof(opt));
#else
                       &opt, sizeof(opt));
#endif
        }
        if (bind(local, (sockaddr*) &m_local_addr, sizeof(m_local_addr)) == ALEXIS_SOCKET_ERROR) {
            wsa_utils::delete_socket(local);
            trans_mesg.exec(0, false, "bind udp socket failed");
            return false;
        }
        m_socket = local;
        trans_mesg.exec(0, true, "open");
        m_thread = std::thread(std::bind(&udp_trans::receive_loop, this));
        return true;
    }
    virtual void close(uint_64 _loc = 0) override {
        if (m_socket.load() != ALEXIS_SOCKET_VALUE) {
            wsa_utils::delete_socket(m_socket.load());
            m_socket.store(ALEXIS_SOCKET_VALUE);
            if (m_thread.joinable()) m_thread.join();
            trans_mesg.exec(0, false, "close");
        }
    }
    virtual bool is_open() override { return m_socket.load() != ALEXIS_SOCKET_VALUE; }
    virtual uint_64 connect_num() override { return m_socket.load() != ALEXIS_SOCKET_VALUE ? 1 : 0; }
    virtual bool bytes_send(const void* _data, const uint_64 _size, uint_64 _loc) override {
        if (m_socket.load() == ALEXIS_SOCKET_VALUE) return false;
        if (nullptr == _data || 0 == _size) return true;
        if (_loc == 0) {
            trans_mesg.exec(0, false, "destination address not specified");
            return false;
        }

        return wsa_utils::send_data_to(m_socket.load(), _data, _size, _loc);
    }

private:
    void receive_loop() {
        bytes buffer(wsa_utils::min_recv_buffer_size);

        while (m_socket.load() != ALEXIS_SOCKET_VALUE) {
            sockaddr_in recv_addr;
            SOCKET_ADDR_LEN addr_len = sizeof(sockaddr_in);
            int recv_size = recvfrom(m_socket.load(), (char*) buffer.data(), (int) buffer.size(), 0,
                                     (sockaddr*) &recv_addr, &addr_len);
            if (wsa_utils::io_interrupted(recv_size)) continue;
            if (recv_size <= 0) continue;
            bytes_recv.exec(buffer.left_view(recv_size), wsa_utils::sockaddr_2_uint64(recv_addr));
            wsa_utils::adjust_buffer(buffer, recv_size);
        }
    }

private:
    std::atomic<socket_t> m_socket{ALEXIS_SOCKET_VALUE};
    std::thread m_thread;
    sockaddr_in m_local_addr;
};

static std::string make_local_path(const std::string& _addr_port) {
    std::pair<uint_32, uint_16> ip = transmit::from_strip(_addr_port);
#ifdef _WIN32
    char temp[MAX_PATH];
    GetTempPathA(MAX_PATH, temp);
    return std::string(temp) + "alx_" + std::to_string(ip.first) + "_" + std::to_string(ip.second) + ".sock";
#else
    return "/tmp/alx_" + std::to_string(ip.first) + "_" + std::to_string(ip.second) + ".sock";
#endif
}

static sockaddr_un make_sockaddr_un(const std::string& _path) {
    sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    size_t len = _path.size() < sizeof(addr.sun_path) - 1 ? _path.size() : sizeof(addr.sun_path) - 1;
    memcpy(addr.sun_path, _path.c_str(), len);
    addr.sun_path[len] = '\0';
    return addr;
}

class local_server
    : public transmit_product<local_server> {
public:
    local_server(const std::string& _addr_port, const varmap&)
        : transmit_product<local_server>(_addr_port, varmap()), m_path(make_local_path(_addr_port)) {
        if (0 != wsa_utils::init_wsa()) {
            trans_mesg.exec(0, false, "wsa start up error");
        }
    }
    ~local_server() {
        close();
        wsa_utils::uninit_wsa();
    }

public:
    virtual bool open() override {
        if (m_server.load() != ALEXIS_SOCKET_VALUE) return true;

        socket_t server = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (server == ALEXIS_SOCKET_VALUE) {
            trans_mesg.exec(0, false, "failed to create local server socket");
            return false;
        }

        sockaddr_un addr = make_sockaddr_un(m_path);
        // unlink a leftover socket file, but only once the probe shows nothing answers on that path
        if (::bind(server, (sockaddr*) &addr, sizeof(addr)) == ALEXIS_SOCKET_ERROR) {
#ifndef _WIN32
            int probe = ::socket(AF_UNIX, SOCK_STREAM, 0);
            int flags = ::fcntl(probe, F_GETFL, 0);
            ::fcntl(probe, F_SETFL, flags | O_NONBLOCK);
            if (::connect(probe, (sockaddr*) &addr, sizeof(addr)) == 0) {
                ::close(probe);
                wsa_utils::delete_socket(server);
                trans_mesg.exec(0, false, "local address already in use");
                return false;
            }
            ::close(probe);
            ::unlink(m_path.c_str());
            if (::bind(server, (sockaddr*) &addr, sizeof(addr)) == ALEXIS_SOCKET_ERROR) {
                trans_mesg.exec(0, false, "bind failed after unlink");
                wsa_utils::delete_socket(server);
                return false;
            }
#else
            int probe = ::socket(AF_UNIX, SOCK_STREAM, 0);
            u_long mode = 1;
            ::ioctlsocket(probe, FIONBIO, &mode);
            if (::connect(probe, (sockaddr*) &addr, sizeof(addr)) == 0) {
                ::closesocket(probe);
                wsa_utils::delete_socket(server);
                trans_mesg.exec(0, false, "local address already in use");
                return false;
            }
            ::closesocket(probe);
            remove(m_path.c_str());
            if (::bind(server, (sockaddr*) &addr, sizeof(addr)) == ALEXIS_SOCKET_ERROR) {
                trans_mesg.exec(0, false, "bind failed after remove");
                wsa_utils::delete_socket(server);
                return false;
            }
#endif
        }

        if (::listen(server, SOMAXCONN) == ALEXIS_SOCKET_ERROR) {
            trans_mesg.exec(0, false, "listen failed");
            wsa_utils::delete_socket(server);
            remove(m_path.c_str());
            return false;
        }

        m_server.store(server);
        trans_mesg.exec(0, true, "open");
        m_thread = new std::thread(std::bind(&local_server::accept_loop, this));
        return true;
    }
    virtual void close(uint_64 _loc = 0) override {
        if (m_server.load() == ALEXIS_SOCKET_VALUE) return;

        if (_loc == 0) {
            wsa_utils::delete_socket(m_server.load());
            m_server.store(ALEXIS_SOCKET_VALUE);

            socket_t client = m_client.exchange(ALEXIS_SOCKET_VALUE);
            if (client != ALEXIS_SOCKET_VALUE) {
                wsa_utils::delete_socket(client);
            }

            if (nullptr != m_thread && m_thread->joinable()) {
                m_thread->join();
                safe_delete(m_thread);
            }
            remove(m_path.c_str());
            trans_mesg.exec(0, false, "close");
        } else {
            // socket taken out first: the receive loop's tail then reports no disconnect of its own
            uint_64 vid = m_vid.exchange(0);
            if (vid != 0 && vid == _loc) {
                socket_t client = m_client.exchange(ALEXIS_SOCKET_VALUE);
                if (client != ALEXIS_SOCKET_VALUE) {
                    wsa_utils::delete_socket(client);
                    trans_mesg.exec(vid, false, "disconnect");
                }
            }
        }
    }
    virtual bool is_open() override { return m_server.load() != ALEXIS_SOCKET_VALUE; }
    virtual uint_64 connect_num() override { return m_vid.load() != 0 ? 1 : 0; }
    virtual bool bytes_send(const void* _data, const uint_64 _size, uint_64 _loc) override {
        if (_loc != 0 && _loc != m_vid.load()) return false;
        socket_t sock = m_client.load();
        if (sock == ALEXIS_SOCKET_VALUE) return false;
        if (nullptr == _data || 0 == _size) return true;
        return wsa_utils::send_data(sock, _data, _size);
    }

private:
    void accept_loop() {
        static std::atomic<uint_64> s_next_id{1};
        while (m_server.load() != ALEXIS_SOCKET_VALUE) {
            sockaddr_un client_addr;
            SOCKET_ADDR_LEN addr_len = sizeof(sockaddr_un);
            socket_t client = ::accept(m_server.load(), (sockaddr*) &client_addr, &addr_len);
            if (client == ALEXIS_SOCKET_VALUE) return;
            wsa_utils::set_send_timeout(client);

            uint_64 vid = s_next_id.fetch_add(1);
            m_vid.store(vid);
            m_client.store(client);
            if (m_server.load() == ALEXIS_SOCKET_VALUE) {
                m_vid.store(0);
                wsa_utils::delete_socket(m_client.exchange(ALEXIS_SOCKET_VALUE));
                return;
            }
            trans_mesg.exec(vid, true, "connect");
            receive_loop();
        }
    }
    void receive_loop() {
        bytes buffer(wsa_utils::min_recv_buffer_size);
        while (true) {
            socket_t sock = m_client.load();
            if (sock == ALEXIS_SOCKET_VALUE) break;
            int recv_size = ::recv(sock, (char*) buffer.data(), (int) buffer.size(), 0);
            if (wsa_utils::io_interrupted(recv_size)) continue;
            if (recv_size <= 0) break;
            bytes_recv.exec(buffer.left_view(recv_size), m_vid.load());
            wsa_utils::adjust_buffer(buffer, recv_size);
        }
        uint_64 vid = m_vid.exchange(0);
        socket_t client = m_client.exchange(ALEXIS_SOCKET_VALUE);
        if (client != ALEXIS_SOCKET_VALUE) {
            wsa_utils::delete_socket(client);
            trans_mesg.exec(vid, false, "disconnect");
        }
    }

private:
    std::atomic<socket_t> m_server{ALEXIS_SOCKET_VALUE};
    std::atomic<socket_t> m_client{ALEXIS_SOCKET_VALUE};
    std::atomic<uint_64> m_vid{0};
    std::thread* m_thread{nullptr};
    std::string m_path;
};

class local_client
    : public transmit_product<local_client> {
public:
    local_client(const std::string& _addr_port, const varmap&)
        : transmit_product<local_client>(_addr_port, varmap()), m_path(make_local_path(_addr_port)) {
        if (0 != wsa_utils::init_wsa()) {
            trans_mesg.exec(0, false, "wsa start up error");
        }
    }
    ~local_client() {
        close();
        wsa_utils::uninit_wsa();
    }

public:
    virtual bool open() override {
        if (m_client.load() != ALEXIS_SOCKET_VALUE) return true;
        release_receiver(m_thread);

        socket_t client;
        if ((client = ::socket(AF_UNIX, SOCK_STREAM, 0)) == ALEXIS_SOCKET_VALUE) {
            trans_mesg.exec(0, false, "failed to create local client socket");
            return false;
        }

        sockaddr_un addr = make_sockaddr_un(m_path);
        if (::connect(client, (sockaddr*) &addr, sizeof(addr)) == ALEXIS_SOCKET_ERROR) {
            wsa_utils::delete_socket(client);
            trans_mesg.exec(0, false, "connection failed");
            return false;
        }
        wsa_utils::set_send_timeout(client);

        m_client.store(client);
        trans_mesg.exec(0, true, "open");
        m_thread = new std::thread(std::bind(&local_client::receive_loop, this));
        return true;
    }
    virtual void close(uint_64 _loc = 0) override {
        socket_t sock = m_client.exchange(ALEXIS_SOCKET_VALUE);
        if (sock != ALEXIS_SOCKET_VALUE) wsa_utils::delete_socket(sock);
        release_receiver(m_thread);
        if (sock != ALEXIS_SOCKET_VALUE) trans_mesg.exec(0, false, "close");
    }
    virtual bool is_open() override { return m_client.load() != ALEXIS_SOCKET_VALUE; }
    virtual uint_64 connect_num() override { return m_client.load() != ALEXIS_SOCKET_VALUE ? 1 : 0; }
    virtual bool bytes_send(const void* _data, const uint_64 _size, uint_64 _loc) override {
        socket_t sock = m_client.load();
        if (sock == ALEXIS_SOCKET_VALUE) return false;
        if (nullptr == _data || 0 == _size) return true;
        return wsa_utils::send_data(sock, _data, _size);
    }

private:
    void receive_loop() {
        bytes buffer(wsa_utils::min_recv_buffer_size);
        while (true) {
            socket_t sock = m_client.load();
            if (sock == ALEXIS_SOCKET_VALUE) break;
            int recv_size = ::recv(sock, (char*) buffer.data(), (int) buffer.size(), 0);
            if (wsa_utils::io_interrupted(recv_size)) continue;
            if (recv_size <= 0) break;
            bytes_recv.exec(buffer.left_view(recv_size), 0);
            wsa_utils::adjust_buffer(buffer, recv_size);
        }
        socket_t sock = m_client.exchange(ALEXIS_SOCKET_VALUE);
        if (sock != ALEXIS_SOCKET_VALUE) {
            wsa_utils::delete_socket(sock);
            trans_mesg.exec(0, false, "close");
        }
    }

private:
    std::atomic<socket_t> m_client{ALEXIS_SOCKET_VALUE};
    std::thread* m_thread{nullptr};
    std::string m_path;
};

#if ALEXIS_USE_TLS

#    include <openssl/err.h>
#    include <openssl/ssl.h>
#    include <openssl/x509v3.h>

#    include <csignal>
#    include <mutex>

namespace tls_utils {

    // SIGPIPE from a dead peer would kill the process; only the default disposition is taken over
    static void init_signal() {
        static std::once_flag once;
        std::call_once(once, [] {
#    ifdef _WIN32
#    else
            struct sigaction sa = {};
            if (0 != sigaction(SIGPIPE, nullptr, &sa) || SIG_DFL != sa.sa_handler) return;
            sa.sa_handler = SIG_IGN;
            sa.sa_flags = 0;
            sigaction(SIGPIPE, &sa, nullptr);
#    endif
        });
    }
    static int tls_poll(socket_t _sock, bool _want_write, int _timeout_ms) {
#    ifdef _WIN32
        WSAPOLLFD pfd = {_sock, (short) (_want_write ? POLLOUT : POLLIN), 0};
        return WSAPoll(&pfd, 1, _timeout_ms);
#    else
        pollfd pfd = {(int) _sock, (short) (_want_write ? POLLOUT : POLLIN), 0};
        return ::poll(&pfd, 1, _timeout_ms);
#    endif
    }

    static bool set_verify_host(SSL* _ssl, const std::string& _host) {
        X509_VERIFY_PARAM* param = SSL_get0_param(_ssl);
        X509_VERIFY_PARAM_set_hostflags(param, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
        unsigned char addr[16];
        if (1 == inet_pton(AF_INET, _host.c_str(), addr) || 1 == inet_pton(AF_INET6, _host.c_str(), addr))
            return 1 == X509_VERIFY_PARAM_set1_ip_asc(param, _host.c_str());
        return 1 == X509_VERIFY_PARAM_set1_host(param, _host.c_str(), 0);
    }
    static void shutdown_socket(socket_t _socket) {
        if (_socket == ALEXIS_SOCKET_VALUE) return;
#    ifdef _WIN32
        ::shutdown(_socket, SD_BOTH);
#    else
        ::shutdown(_socket, SHUT_RDWR);
#    endif
    }
    static bool send_data(SSL* _ssl, socket_t _sock, const void* _data, const uint_64 _size) {
        uint_64 offset{0};
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(wsa_utils::send_timeout_ms);
        while (_size > offset) {
            int ret = SSL_write(_ssl, (const unsigned char*) _data + offset,
                                (int) (_size - offset));
            if (ret > 0) {
                offset += ret;
                // inactivity bound, not a total one: every byte written restarts the deadline
                deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(wsa_utils::send_timeout_ms);
                continue;
            }
            int err = SSL_get_error(_ssl, ret);
            if (err == SSL_ERROR_WANT_WRITE || err == SSL_ERROR_WANT_READ) {

                if (std::chrono::steady_clock::now() > deadline) return false;
                tls_poll(_sock, err == SSL_ERROR_WANT_WRITE, 5);
                continue;
            }
            return false;
        }
        return true;
    }
};

class tls_server
    : public transmit_product<tls_server> {
public:
    // one SSL object per peer: mtx serializes the reader thread against bytes_send, ready marks the handshake done
    struct entry {
        socket_t sock{ALEXIS_SOCKET_VALUE};
        SSL* ssl{nullptr};
        std::mutex mtx;
        bool ready{false};
    };

    tls_server(const std::string& _addr_port, const varmap& _ext_info)
        : transmit_product<tls_server>(_addr_port, _ext_info) {
        if (0 != wsa_utils::init_wsa()) {
            trans_mesg.exec(0, false, "wsa start up error");
        }
        const std::pair<uint_32, uint_16> ip = from_strip(_addr_port);
        m_server_addr.sin_family = AF_INET;
        m_server_addr.sin_addr.S_ADDR = htonl(ip.first);
        m_server_addr.sin_port = htons(ip.second);

        m_cert_path = _ext_info.value("cert").to<std::string>("");
        m_key_path = _ext_info.value("key").to<std::string>("");
        m_ca_path = _ext_info.value("ca").to<std::string>("");
        m_alpn = _ext_info.value("alpn").to<std::string>("");
    }
    ~tls_server() {
        close();
        wsa_utils::uninit_wsa();
    }

public:
    virtual bool open() override {
        if (m_server.load() != ALEXIS_SOCKET_VALUE) return true;

        tls_utils::init_signal();
        m_ctx = SSL_CTX_new(TLS_server_method());
        if (nullptr == m_ctx) {
            trans_mesg.exec(0, false, "SSL_CTX_new failed");
            return false;
        }
        if (m_cert_path.empty() ||
            1 != SSL_CTX_use_certificate_chain_file(m_ctx, m_cert_path.c_str())) {
            char buf[256];
            snprintf(buf, sizeof(buf), "cert load fail: %s", m_cert_path.c_str());
            trans_mesg.exec(0, false, buf);
            goto tls_error;
        }
        if (m_key_path.empty() ||
            1 != SSL_CTX_use_PrivateKey_file(m_ctx, m_key_path.c_str(), SSL_FILETYPE_PEM)) {
            trans_mesg.exec(0, false, "failed to load server private key");
            goto tls_error;
        }

        if (!m_alpn.empty()) {
            std::list<std::string> parts = strutil::split(m_alpn, ",");
            std::vector<unsigned char> alpn_buf;
            for (auto& s : parts) {
                alpn_buf.push_back((unsigned char) s.size());
                alpn_buf.insert(alpn_buf.end(), s.begin(), s.end());
            }
            SSL_CTX_set_alpn_protos(m_ctx, alpn_buf.data(), (unsigned int) alpn_buf.size());
        }

        if (!m_ca_path.empty()) {
            SSL_CTX_load_verify_locations(m_ctx, m_ca_path.c_str(), nullptr);
            SSL_CTX_set_verify(m_ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, nullptr);
        }

        if ((m_server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == ALEXIS_SOCKET_VALUE) {
            trans_mesg.exec(0, false, "failed to create server socket");
            goto tls_error;
        }
        {
            int opt = 1;
            setsockopt(m_server, SOL_SOCKET, SO_REUSEADDR,
#    ifdef _WIN32
                       (const char*) &opt, sizeof(opt));
#    else
                       &opt, sizeof(opt));
#    endif
        }
        if (bind(m_server.load(), (sockaddr*) &m_server_addr, sizeof(m_server_addr)) == ALEXIS_SOCKET_ERROR ||
            listen(m_server.load(), SOMAXCONN) == ALEXIS_SOCKET_ERROR) {
            trans_mesg.exec(0, false, "bind or listen port failed");
            wsa_utils::delete_socket(m_server.load());
            m_server = ALEXIS_SOCKET_VALUE;
            goto tls_error;
        }

        m_tls_ok = true;
        trans_mesg.exec(0, true, "open");
        m_thread = new std::thread(std::bind(&tls_server::connect_server, this));
        return true;

    tls_error:
        if (m_ctx) {
            SSL_CTX_free(m_ctx);
            m_ctx = nullptr;
        }
        return false;
    }
    virtual void close(uint_64 _loc = 0) override {
        if (m_server.load() != ALEXIS_SOCKET_VALUE) {
            if (0 == _loc) {
                wsa_utils::delete_socket(m_server.load());
                m_server.store(ALEXIS_SOCKET_VALUE);
                wake_workers();
                if (nullptr != m_thread && m_thread->joinable()) {
                    m_thread->join();
                    safe_delete(m_thread);
                }
                wake_workers();
                join_workers();
                if (m_tls_ok) {
                    if (m_ctx) {
                        SSL_CTX_free(m_ctx);
                        m_ctx = nullptr;
                    }
                    m_tls_ok = false;
                }
                trans_mesg.exec(0, false, "close");
            } else {
                _WRITE_LOCK_(m_mutex);
                auto iter = m_clients.find(_loc);
                if (iter != m_clients.end()) {
                    tls_utils::shutdown_socket(iter->second->sock);
                }
            }
        }
    }
    virtual bool is_open() override { return m_server.load() != ALEXIS_SOCKET_VALUE; }
    virtual uint_64 connect_num() override {
        _READ__LOCK_(m_mutex);
        return m_clients.size();
    }
    virtual bool bytes_send(const void* _data, const uint_64 _size, uint_64 _loc) override {
        if (nullptr == _data || 0 == _size || connect_num() == 0) return true;
        uint_64 sent_count{0}, ready_count{0};
        {
            _READ__LOCK_(m_mutex);
            for (auto& client : m_clients) {
                if (0 != _loc && _loc != client.first) continue;
                std::lock_guard<std::mutex> lock(client.second->mtx);
                if (!client.second->ready) continue;
                ready_count++;
                if (tls_utils::send_data(client.second->ssl, client.second->sock, _data, _size))
                    sent_count++;
            }
        }
        return 0 == ready_count || 0 != sent_count;
    }

private:
    void connect_server() {
        while (m_server.load() != ALEXIS_SOCKET_VALUE) {
            sockaddr_in client_addr;
            SOCKET_ADDR_LEN addr_len = sizeof(sockaddr_in);
            socket_t client = accept(m_server.load(), (sockaddr*) &client_addr, &addr_len);
            if (ALEXIS_SOCKET_VALUE == client) return;
            uint_32 addr = ntohl(client_addr.sin_addr.S_ADDR);
            uint_16 port = ntohs(client_addr.sin_port);
            const uint_64 key = ((uint_64) addr << 32) | (uint_64) port;

            SSL* ssl = SSL_new(m_ctx);
            if (nullptr == ssl) {
                wsa_utils::delete_socket(client);
                continue;
            }
            SSL_set_fd(ssl, (int) client);
            wsa_utils::set_send_timeout(client);
            wsa_utils::set_recv_timeout(client, wsa_utils::handshake_timeout_ms);

            // registered before the handshake so close() can wake this peer; a silent peer stalls only its own thread
            std::shared_ptr<entry> e = std::make_shared<entry>();
            e->sock = client;
            e->ssl = ssl;
            {
                _WRITE_LOCK_(m_mutex);
                m_clients[key] = e;
            }
            std::shared_ptr<std::atomic<bool>> done = std::make_shared<std::atomic<bool>>(false);
            std::thread worker(std::bind(&tls_server::receive_client, this, e, key, done));
            {
                _WRITE_LOCK_(m_mutex);
                m_workers.emplace_back(done, std::move(worker));
            }
            reap_workers();
        }
    }
    void receive_client(std::shared_ptr<entry> _e, uint_64 _key, std::shared_ptr<std::atomic<bool>> _done) {
        ERR_clear_error();
        int hs_ret = SSL_accept(_e->ssl);
        if (1 != hs_ret) {
            int err = SSL_get_error(_e->ssl, hs_ret);
            char buf[512];
            snprintf(buf, sizeof(buf), "ssl_err=%d cert=%s key=%s", err, m_cert_path.c_str(), m_key_path.c_str());
            trans_mesg.exec(_key, false, buf);

            ERR_print_errors_fp(stderr);
            cleanup_client(_e, _key, _done);
            return;
        }
        {
            std::lock_guard<std::mutex> lock(_e->mtx);
            _e->ready = true;
        }
        trans_mesg.exec(_key, true, "connect");

#    ifdef _WIN32
        {
            u_long mode = 1;
            ioctlsocket(_e->sock, FIONBIO, &mode);
        }
#    else
        {
            int flags = fcntl(_e->sock, F_GETFL, 0);
            fcntl(_e->sock, F_SETFL, flags | O_NONBLOCK);
        }
#    endif

        bytes buffer(wsa_utils::min_recv_buffer_size);
        while (true) {
            int ret;
            int err;
            {

                std::lock_guard<std::mutex> lock(_e->mtx);
                ret = SSL_read(_e->ssl, buffer.data(), buffer.size());
                err = SSL_get_error(_e->ssl, ret);
            }
            if (ret > 0) {
                bytes_recv.exec(buffer.left_view(ret), _key);
                wsa_utils::adjust_buffer(buffer, ret);
                continue;
            }
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
                tls_utils::tls_poll(_e->sock, err == SSL_ERROR_WANT_WRITE, 5);
                continue;
            }
            break;
        }
        cleanup_client(_e, _key, _done);
    }
    void cleanup_client(std::shared_ptr<entry>& _e, uint_64 _key, std::shared_ptr<std::atomic<bool>> _done) {
        SSL* ssl = nullptr;
        {
            std::lock_guard<std::mutex> lock(_e->mtx);
            ssl = _e->ssl;
            _e->ssl = nullptr;
            _e->ready = false;
        }
        if (nullptr != ssl) {
            SSL_shutdown(ssl);
            SSL_free(ssl);
        }
        {
            _WRITE_LOCK_(m_mutex);
            auto iter = m_clients.find(_key);
            if (iter != m_clients.end() && iter->second == _e) m_clients.erase(iter);
        }
        // erased above: close(loc) reaches this fd through m_clients, so the entry must go first
        wsa_utils::delete_socket(_e->sock);
        trans_mesg.exec(_key, false, "disconnect");
        _done->store(true);
    }

private:

    void wake_workers() {
        _READ__LOCK_(m_mutex);
        for (auto& client : m_clients) tls_utils::shutdown_socket(client.second->sock);
    }
    void join_workers() {
        std::list<worker_slot> workers;
        {
            _WRITE_LOCK_(m_mutex);
            workers.splice(workers.end(), m_workers);
        }
        join_threads(workers);
    }

    void reap_workers() {
        std::list<worker_slot> workers;
        {
            _WRITE_LOCK_(m_mutex);
            for (auto iter = m_workers.begin(); iter != m_workers.end();) {
                if (iter->first->load()) workers.splice(workers.end(), m_workers, iter++);
                else ++iter;
            }
        }
        join_threads(workers);
    }

private:
    SSL_CTX* m_ctx{nullptr};
    bool m_tls_ok{false};
    std::string m_cert_path;
    std::string m_key_path;
    std::string m_ca_path;
    std::string m_alpn;

    std::atomic<socket_t> m_server{ALEXIS_SOCKET_VALUE};
    sockaddr_in m_server_addr;
    SHARED_MUTEX m_mutex;
    std::thread* m_thread{nullptr};

    std::unordered_map<uint_64, std::shared_ptr<entry>> m_clients;
    std::list<worker_slot> m_workers;
};

class tls_client
    : public transmit_product<tls_client> {
public:
    tls_client(const std::string& _addr_port, const varmap& _ext_info)
        : transmit_product<tls_client>(_addr_port, _ext_info) {
        if (0 != wsa_utils::init_wsa()) {
            trans_mesg.exec(0, false, "wsa start up error");
        }
        const std::pair<uint_32, uint_16> ip = from_strip(_addr_port);
        m_server_addr.sin_family = AF_INET;
        m_server_addr.sin_addr.S_ADDR = htonl(ip.first);
        m_server_addr.sin_port = htons(ip.second);

        m_ca_path = _ext_info.value("ca").to<std::string>("");
        m_hostname = _ext_info.value("hostname").to<std::string>("");
        m_cert_path = _ext_info.value("cert").to<std::string>("");
        m_key_path = _ext_info.value("key").to<std::string>("");
        m_alpn = _ext_info.value("alpn").to<std::string>("");
    }
    ~tls_client() {
        close();
        wsa_utils::uninit_wsa();
    }

public:
    virtual bool open() override {
        if (m_client.load() != ALEXIS_SOCKET_VALUE) return true;
        release_receiver(m_thread);
        release_tls();

        tls_utils::init_signal();
        m_ctx = SSL_CTX_new(TLS_client_method());
        if (nullptr == m_ctx) {
            trans_mesg.exec(0, false, "SSL_CTX_new failed");
            return false;
        }

        socket_t sock = ALEXIS_SOCKET_VALUE;
        if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == ALEXIS_SOCKET_VALUE) {
            trans_mesg.exec(0, false, "failed to create client socket");
            SSL_CTX_free(m_ctx);
            m_ctx = nullptr;
            return false;
        }
        if (connect(sock, (sockaddr*) &m_server_addr, sizeof(m_server_addr)) == ALEXIS_SOCKET_ERROR) {
            wsa_utils::delete_socket(sock);
            trans_mesg.exec(0, false, "connection failed");
            SSL_CTX_free(m_ctx);
            m_ctx = nullptr;
            return false;
        }

        m_ssl = SSL_new(m_ctx);
        if (nullptr == m_ssl) {
            trans_mesg.exec(0, false, "SSL_new failed");
            goto error;
        }
        SSL_set_fd(m_ssl, (int) sock);

        if (!m_alpn.empty()) {
            std::list<std::string> parts = strutil::split(m_alpn, ",");
            std::vector<unsigned char> alpn_buf;
            for (auto& s : parts) {
                alpn_buf.push_back((unsigned char) s.size());
                alpn_buf.insert(alpn_buf.end(), s.begin(), s.end());
            }
            SSL_set_alpn_protos(m_ssl, alpn_buf.data(), (unsigned int) alpn_buf.size());
        }

        if (!m_ca_path.empty()) {
            SSL_CTX_load_verify_locations(m_ctx, m_ca_path.c_str(), nullptr);
            SSL_set_verify(m_ssl, SSL_VERIFY_PEER, nullptr);

            if (m_hostname.empty()) {
                trans_mesg.exec(0, false, "ca configured without hostname: peer identity cannot be verified");
                goto error;
            }
            if (!tls_utils::set_verify_host(m_ssl, m_hostname)) {
                trans_mesg.exec(0, false, "failed to set expected peer hostname");
                goto error;
            }
        } else {
            SSL_set_verify(m_ssl, SSL_VERIFY_NONE, nullptr);
        }

        if (!m_cert_path.empty() && !m_key_path.empty()) {
            if (1 != SSL_use_certificate_chain_file(m_ssl, m_cert_path.c_str()) ||
                1 != SSL_use_PrivateKey_file(m_ssl, m_key_path.c_str(), SSL_FILETYPE_PEM)) {
                trans_mesg.exec(0, false, "failed to load client certificate");
                goto error;
            }
        }

        if (!m_hostname.empty()) {
            SSL_set_tlsext_host_name(m_ssl, m_hostname.c_str());
        }

        {
            int hs_ret = SSL_connect(m_ssl);
            if (1 != hs_ret) {
                int err = SSL_get_error(m_ssl, hs_ret);
                char buf[64];
                snprintf(buf, sizeof(buf), "TLS handshake failed: %d", err);
                trans_mesg.exec(0, false, buf);
                goto error;
            }
        }

#    ifdef _WIN32
        {
            u_long mode = 1;
            ioctlsocket(sock, FIONBIO, &mode);
        }
#    else
        {
            int flags = fcntl(sock, F_GETFL, 0);
            fcntl(sock, F_SETFL, flags | O_NONBLOCK);
        }
#    endif

        m_tls_ok = true;
        m_client.store(sock);
        trans_mesg.exec(0, true, "open");
        m_thread = new std::thread(std::bind(&tls_client::receive_client, this));
        return true;

    error:
        if (m_ssl) {
            SSL_free(m_ssl);
            m_ssl = nullptr;
        }
        if (m_ctx) {
            SSL_CTX_free(m_ctx);
            m_ctx = nullptr;
        }
        wsa_utils::delete_socket(sock);
        return false;
    }
    virtual void close(uint_64 _loc = 0) override {
        socket_t sock = m_client.exchange(ALEXIS_SOCKET_VALUE);

        // shutdown before the join below: the receiver may still be blocked in SSL_read on this fd
        if (sock != ALEXIS_SOCKET_VALUE) wsa_utils::shutdown_socket(sock);
        release_receiver(m_thread);
        wsa_utils::delete_socket(sock);
        release_tls();
        if (sock != ALEXIS_SOCKET_VALUE) trans_mesg.exec(0, false, "close");
    }
    virtual bool is_open() override { return m_client.load() != ALEXIS_SOCKET_VALUE; }
    virtual uint_64 connect_num() override { return m_client.load() != ALEXIS_SOCKET_VALUE ? 1 : 0; }
    virtual bool bytes_send(const void* _data, const uint_64 _size, uint_64 _loc) override {
        socket_t sock = m_client.load();
        if (sock == ALEXIS_SOCKET_VALUE) return false;
        if (nullptr == _data || 0 == _size) return true;
        std::lock_guard<std::mutex> lock(m_ssl_mtx);
        if (nullptr == m_ssl) return false;
        return tls_utils::send_data(m_ssl, sock, _data, _size);
    }

private:
    void receive_client() {
        bytes buffer(wsa_utils::min_recv_buffer_size);
        while (true) {
            socket_t sock = m_client.load();
            if (sock == ALEXIS_SOCKET_VALUE) break;
            std::lock_guard<std::mutex> lock(m_ssl_mtx);
            int ret = SSL_read(m_ssl, buffer.data(), buffer.size());
            if (ret > 0) {
                bytes_recv.exec(buffer.left_view(ret), 0);
                wsa_utils::adjust_buffer(buffer, ret);
                continue;
            }
            int err = SSL_get_error(m_ssl, ret);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
                tls_utils::tls_poll(sock, err == SSL_ERROR_WANT_WRITE, 5);
                continue;
            }
            break;
        }
        socket_t sock = m_client.exchange(ALEXIS_SOCKET_VALUE);
        if (sock != ALEXIS_SOCKET_VALUE) {
            wsa_utils::delete_socket(sock);
            trans_mesg.exec(0, false, "close");
        }
    }

private:

    void release_tls() {
        std::lock_guard<std::mutex> lock(m_ssl_mtx);
        if (!m_tls_ok) return;
        if (m_ssl) {
            SSL_free(m_ssl);
            m_ssl = nullptr;
        }
        if (m_ctx) {
            SSL_CTX_free(m_ctx);
            m_ctx = nullptr;
        }
        m_tls_ok = false;
    }

private:
    SSL_CTX* m_ctx{nullptr};
    SSL* m_ssl{nullptr};
    std::mutex m_ssl_mtx;
    bool m_tls_ok{false};
    std::string m_ca_path;
    std::string m_hostname;
    std::string m_cert_path;
    std::string m_key_path;
    std::string m_alpn;

    std::atomic<socket_t> m_client{ALEXIS_SOCKET_VALUE};
    std::thread* m_thread{nullptr};
    sockaddr_in m_server_addr;
};

#endif
