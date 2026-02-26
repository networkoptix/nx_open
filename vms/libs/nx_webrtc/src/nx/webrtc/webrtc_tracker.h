// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <rapidjson/document.h>

#include <nx/network/websocket/websocket.h>

#include "webrtc_session.h"

class QnMediaServerModule;

namespace nx::webrtc {

class NX_WEBRTC_API Tracker
{
public:
    Tracker(Session* session);
    ~Tracker();
    void start(
        std::unique_ptr<AbstractStreamSocket>&& socket,
        const nx::network::http::HttpHeaders& headers);
    void onIceCandidate(const IceCandidate& candidate);

    void sendOffer();
    bool isStarted() const { return m_started; }

    std::string idForToStringFromPtr() const;

private:
    void readMessage();
    bool processMessages();
    AnswerResult examineAnswer();
    void sendIce(const IceCandidate& candidate);
    void sendJson(const rapidjson::Document& doc);
    void onBytesRead(SystemError::ErrorCode errorCode, std::size_t /*bytesTransferred*/);
    void stopUnsafe();

private:
    std::optional<IceCandidate> m_srflxCandidate;
    WebSocketPtr m_webSocket;
    Session* m_session = nullptr;
    nx::Buffer m_buffer;
    nx::network::aio::BasicPollable m_pollable;
    IceCandidate::Filter m_candidateFilter = IceCandidate::Filter::All;
    std::atomic<bool> m_started{};
    bool m_iceSent = false;
    bool m_srflxReceived = false;
    std::string m_sessionId;
};

} // namespace nx::webrtc
