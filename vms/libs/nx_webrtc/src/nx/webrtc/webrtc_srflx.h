// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "webrtc_ice.h"

#include <nx/utils/elapsed_timer.h>

namespace nx::webrtc {

/* Srflx (Server Reflexive Candidate) is used to create a NAT Mapping,
 * which also known as UDP hole punching.
 * Usage:
 * 1. Call asyncPunch() for each known STUN server.
 * There are 4 possible scenarios:
 * a) Got at least 2 response before timeout and mapped address is the same.
 * b) Got at least 2 response before timeout and mapped address is varied.
 * c) Got only one response before the timeout.
 * d) No valid response received.
 * Cases a) and c) interpreted as "success", b) and d) - as "failed"
 * 2. Call takeSocket(): for "success" punching it was a socket,
 * for "failed" it will be nullptr.
 * 3. Call getMappedAddress() to provide your mapped address to another peer.
 * 4. Use this socket to create IceUdp, and use it like usual UDP candidate with
 * one exception: IceUdp::sendBinding() should be called to finish punching into peer.
 * */
class NX_WEBRTC_API Srflx
{
public:
    Srflx(
        SessionPool* sessionPool,
        const std::vector<nx::network::SocketAddress>& stunAddress,
        const std::string& sessionId,
        const SessionConfig& config);
    ~Srflx();

    void start();

private:
    void onBytesRead(SystemError::ErrorCode errorCode, std::size_t bytesTransferred);
    void putResult(const nx::network::SocketAddress& socketAddress);

private:
    std::string m_sessionId;
    const SessionConfig m_config;
    std::deque<nx::network::SocketAddress> m_addrQueue;
    nx::utils::ElapsedTimer m_timeout;
    nx::network::aio::BasicPollable m_pollable;
    nx::Buffer m_readBuffer;
    nx::Buffer m_sendBuffer;

    std::unique_ptr<nx::network::AbstractDatagramSocket> m_socket;
    nx::network::SocketAddress m_mappedAddress;
    nx::webrtc::SessionPool* m_sessionPool = nullptr;
};

} // namespace nx::webrtc
