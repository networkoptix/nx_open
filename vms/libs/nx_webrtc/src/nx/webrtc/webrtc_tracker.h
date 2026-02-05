// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <rapidjson/document.h>

#include <nx/network/websocket/websocket.h>
#include <nx/vms/common/system_context_aware.h>

#include "webrtc_session.h"

class QnMediaServerModule;
class SessionPool;

namespace nx::webrtc {

class NX_WEBRTC_API Tracker: public nx::vms::common::SystemContextAware
{
public:
    Tracker(
        nx::vms::common::SystemContext* context,
        SessionPool* sessionPool,
        Session* session);
    ~Tracker();
    void start(
        std::unique_ptr<AbstractStreamSocket>&& socket,
        const nx::network::http::HttpHeaders& headers);
    void onIceCandidate(const IceCandidate& candidate);

private:
    void readMessage();
    bool processMessages();
    AnswerResult examineAnswer();
    void sendOffer();
    void sendIce(const IceCandidate& candidate);
    void sendJson(const rapidjson::Document& doc);
    void onBytesRead(SystemError::ErrorCode errorCode, std::size_t /*bytesTransferred*/);
    void stopUnsafe();

private:
    enum class Stage
    {
        offer,
        answerSuccess,
        srflxReceived,
        srflxSent,
    };

private:
    SessionPool* m_sessionPool;
    std::optional<IceCandidate> m_srflxCandidate;
    WebSocketPtr m_webSocket;
    Session* m_session = nullptr;
    nx::Buffer m_buffer;
    Stage m_stage = Stage::offer;
    nx::network::aio::BasicPollable m_pollable;
    IceCandidate::Filter m_candidateFilter = IceCandidate::Filter::All;
};

} // namespace nx::webrtc
