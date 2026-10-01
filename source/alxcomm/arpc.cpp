/*****************************************************************/ /**
 * \file   arpc.cpp
 * \brief  RPC remote procedure call framework
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "arpc.h"
#include "adatetime.h"
#include "athreadpool.h"

enum RPC_STATE : alx::uint_32 {
    INVALID = 0,
    REQUEST,
    REPLY
};

alx::rpc::rpc(transmit* _trans, uint_64 _pool_size, uint_64 _queue_limit)
    : comm_ex(_trans), m_threadpool(new threadpool(_pool_size, _queue_limit)) {
    data_recv.connect([this](const varmap& _data, uint_64 _loc) { this->on_data_recv(_data, _loc); });
    W_THREAD_SAFE(m_service_map);
    m_service_map.data().insert({0, service_pack::create_noerr(
                                        std::bind(&rpc::list_service, this), "", cmps_option::CMPS_NON_FST, true)});
}

alx::rpc::~rpc() {
    close();
    delete m_threadpool;
}

alx::varmap alx::rpc::list_service() const {
    datetime time;
    varmap vmap;
    R_THREAD_SAFE(m_service_map);
    for (const auto& it : m_service_map.data())
        if (0 != it.first) vmap.insert(std::to_string(it.first), it.second.__describe);
    // the read lock still covers the log emission: a mesg_prit handler must not install or remove a service
    mesg_prit.exec("exec list service, cost: " + std::to_string(time.elapsed_ms()) + "ms");
    return vmap;
}

alx::uint_64 alx::rpc::install_service(const service_pack& _service_pack) {
    static std::atomic<uint_64> s_svid(0);
    W_THREAD_SAFE(m_service_map);
    const auto ret = m_service_map.data().insert({++s_svid, _service_pack});
    return ret.first->first;
}

void alx::rpc::remove_service(const uint_64 _service_svid) {
    W_THREAD_SAFE(m_service_map);
    m_service_map.data().erase(_service_svid);
}

void alx::rpc::consume_service(const consume_pack& _consume_pack) {
    static std::atomic<uint_64> s_rpcid{0};

    const uint_64 crpcid = ++s_rpcid;
    // registered before the send, so a reply is never dispatched without its entry; a send that fails leaves the entry behind
    {
        W_THREAD_SAFE(m_consume_map);
        m_consume_map.data().insert({crpcid, _consume_pack});
    }
    // the two-arg data_send() passes _loc 0: the request goes out on every connection of the transport
    data_send({{"rpcid", crpcid},
               {"state", (uint_32) RPC_STATE::REQUEST},
               {"service", _consume_pack.__svid},
               {"expcmps", (uint_8) _consume_pack.__rst_compress},
               {"data", _consume_pack.__param}},
              _consume_pack.__call_compress);
}

void alx::rpc::on_data_recv(const varmap& _data, uint_64 _loc) {
    const uint_64 rpcid = _data.value("rpcid").to<uint_64>(0);
    const RPC_STATE state = (RPC_STATE) _data.value("state").to<uint_32>(0);
    const uint_64 service = _data.value("service").to<uint_64>();
    const cmps_expect cmps = (cmps_expect) _data.value("expcmps").to<uint_8>((uint_8) cmps_expect::AUTO);

    const std::string error = _data.value("error").to_string();
    const variant data = _data.value("data");
    switch (state) {
    case RPC_STATE::REQUEST: {
        service_pack svc;
        {
            R_THREAD_SAFE(m_service_map);
            auto it = m_service_map.data().find(service);
            if (it != m_service_map.data().end()) svc = it->second;
        }
        if (!svc.valid()) {
            data_send({{"rpcid", rpcid}, {"state", (uint_32) RPC_STATE::REPLY}, {"service", service}, {"error", std::string("service not found")}, {"data", variant()}}, _loc, false);
            return;
        }
        if (svc.__inline_exec)
            on_exec_service(rpcid, svc, data, cmps, _loc);
        else
            m_threadpool->enqueue(std::bind(&rpc::on_exec_service, this, rpcid, svc, data, cmps, _loc));
        return;
    }
    case RPC_STATE::REPLY:
        m_threadpool->enqueue(std::bind(&rpc::on_recv_result, this, rpcid, error, data));
        return;
    case RPC_STATE::INVALID:
    default:
        // unknown state, or a frame carrying no "state" at all: reported on mesg_prit and dropped, with no reply
        return mesg_prit.exec("recv invalid rpc state");
    }
}

void alx::rpc::on_exec_service(uint_64 _rpcid, service_pack _svc, const variant _param, cmps_expect _cmps, uint64_t _loc) {
    variant result;
    std::string error;
    bool cmps{false};

    if (_svc.valid()) {
        try {
            _svc.__function(_param.to<varvec>(), result, error);
            if (cmps_option::NEVER == _svc.__compress || cmps_expect::NEVER == _cmps) cmps = false;
            else if (cmps_option::ALWAYS == _svc.__compress || cmps_expect::ALWAYS == _cmps) cmps = true;
            else cmps = cmps_option::CMPS_FST == _svc.__compress;
        } catch (const std::exception& _ex) {
            error = std::string("exception: ") + _ex.what();
            cmps = false;
        } catch (...) {
            error = "unknown exception";
            cmps = false;
        }
    } else {
        error = "service not found";
        cmps = false;
    }

    data_send({{"rpcid", _rpcid},
               {"state", (uint_32) RPC_STATE::REPLY},
               {"error", error},
               {"data", result}},
              _loc, cmps);
}

void alx::rpc::on_recv_result(uint_64 _rpcid, const std::string _error, const variant _result) {
    consume_pack consume;
    {
        W_THREAD_SAFE(m_consume_map);
        consume_map::iterator iter = m_consume_map.data().find(_rpcid);
        if (iter != m_consume_map.data().end()) {
            consume = iter->second;
            m_consume_map.data().erase(iter);
        }
    }

    // erased and the lock released before the callback runs: it may consume a service itself
    if (consume.valid()) {
        try {
            consume.__function(_result, _error);
        } catch (const std::exception& _ex) {
            mesg_prit.exec(std::string("call consume callback cause exception: ") + _ex.what());
        } catch (...) {
            mesg_prit.exec(std::string("call consume callback cause unknown exception"));
        }
    }
}
