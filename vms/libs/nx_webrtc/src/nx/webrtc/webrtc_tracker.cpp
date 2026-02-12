// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "webrtc_tracker.h"

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <common/common_module.h>
#include <network/tcp_connection_priv.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/websocket/websocket_handshake.h>

#include "webrtc_session_pool.h"
#include "nx_webrtc_ini.h"

namespace nx::webrtc {

using namespace nx::vms::api;
using namespace nx::network;
using namespace nx;

Tracker::Tracker(Session* session):
    m_session(session)
{
    const std::string iceCandidatesIni = ::ini().webrtcIceCandidates;
    if (!iceCandidatesIni.empty())
        m_candidateFilter = IceCandidate::parseFilter(iceCandidatesIni);
}

Tracker::~Tracker()
{
    std::promise<void> stoppedPromise;
    m_pollable.dispatch(
        [this, &stoppedPromise]()
        {
            stopUnsafe();
            stoppedPromise.set_value();
        });
    stoppedPromise.get_future().wait();
    NX_VERBOSE(this, "Destroyed tracker.");
}

void Tracker::stopUnsafe()
{
    if (!m_webSocket)
        return;
    m_webSocket.reset();
    // Dangerous: object does not exists after this call.
    m_session->releaseTracker();
}

void Tracker::start(
    std::unique_ptr<AbstractStreamSocket>&& socket,
    const nx::network::http::HttpHeaders& headers)
{
    socket->setNonBlockingMode(true);
    socket->bindToAioThread(m_pollable.getAioThread());

    m_webSocket.reset(
        new WebSocket(
            std::move(socket),
            nx::network::websocket::Role::server,
            nx::network::websocket::FrameType::text,
            websocket::compressionType(headers)));
    m_pollable.post(
        [this]()
        {
            m_webSocket->start();
            sendOffer();
            readMessage();
            m_started = true;
        });
}

void Tracker::readMessage()
{
    m_webSocket->readSomeAsync(
        &m_buffer,
        [this](auto... args)
        {
            onBytesRead(std::move(args)...);
        });
}

void Tracker::sendOffer()
{
    rapidjson::Document d(rapidjson::kObjectType);
    rapidjson::Value sdpRoot(rapidjson::kObjectType);
    sdpRoot.AddMember("type", "offer", d.GetAllocator());
    std::string sdp = m_session->constructSdp();
    rapidjson::Value sdpValue(sdp, d.GetAllocator());
    sdpRoot.AddMember("sdp", sdpValue, d.GetAllocator());
    if (const auto& mime = m_session->tracks()->mimeType(); !mime.empty())
    {
        rapidjson::Value mimeValue(mime, d.GetAllocator());
        d.AddMember("mime", mimeValue, d.GetAllocator());
    }

    d.AddMember("sdp", sdpRoot, d.GetAllocator());
    sendJson(d);
}

void Tracker::sendJson(const rapidjson::Document& doc)
{
    nx::Buffer buf;
    rapidjson::StringBuffer b;
    rapidjson::Writer<rapidjson::StringBuffer> w(b);
    doc.Accept(w);
    std::string s(b.GetString(), b.GetSize());
    NX_VERBOSE(this, "Send json: %1", s);
    buf.resize(s.size());
    memcpy(buf.begin(), s.data(), s.size());
    m_webSocket->sendAsync(
        &buf,
        [this] (SystemError::ErrorCode error, size_t /*size*/)
        {
            if (error != SystemError::noError)
            {
                NX_VERBOSE(this, "Got send error %1", SystemError::toString(error));
                stopUnsafe();
            }
        });
}

void Tracker::sendIce(const IceCandidate& candidate)
{
    if (!candidate.matchFilter(m_candidateFilter))
        return;
    auto candidateStr = candidate.serialize();
    rapidjson::Document d(rapidjson::kObjectType);
    rapidjson::Value ice(rapidjson::kObjectType);
    rapidjson::Value candidateValue(candidateStr, d.GetAllocator());
    ice.AddMember("candidate", candidateValue, d.GetAllocator());

    const auto tracks = m_session->tracks()->allTracks();
    int mid = tracks.empty() ? kDataChannelMid : tracks[0].mid;
    ice.AddMember("sdpMid", mid, d.GetAllocator());
    ice.AddMember("sdpMLineIndex", 0, d.GetAllocator());
    std::string ufrag = m_session->id();
    rapidjson::Value ufragValue(ufrag, d.GetAllocator());
    ice.AddMember("usernameFragment", ufragValue, d.GetAllocator());
    d.AddMember("ice", ice, d.GetAllocator());
    sendJson(d);
}

AnswerResult Tracker::examineAnswer()
{
    rapidjson::Document d;
    const auto s = m_buffer.toString();
    d.Parse(s.data());
    if (!d.IsObject())
        return AnswerResult::noop;
    if (d.HasMember("ice") && m_stage != Stage::offer)
    {
        const auto& ice = d["ice"];
        if (ice.IsObject() && ice.HasMember("candidate"))
        {
            const auto& candidate = ice["candidate"];
            if (candidate.IsString())
            {
                auto iceCandidate = IceCandidate::parse(candidate.GetString());

                if (iceCandidate.type == IceCandidate::Type::Srflx)
                    m_session->gotIceFromTracker(iceCandidate);

                if (iceCandidate.type == IceCandidate::Type::Srflx)
                {
                    m_stage = Stage::srflxReceived;
                    if (m_srflxCandidate)
                    {
                        sendIce(*m_srflxCandidate);
                        m_stage = Stage::srflxSent;
                    }
                }
            }
        }
    }
    if (d.HasMember("error"))
        return m_session->setFallbackCodecs();
    if (!d.HasMember("sdp"))
        return AnswerResult::noop; //< Case with remote ice-candidate received.
    const auto& sdpRoot = d["sdp"];
    if (!sdpRoot.IsObject() || !sdpRoot.HasMember("type") || !sdpRoot.HasMember("sdp"))
        return AnswerResult::noop;
    const auto& msgType = sdpRoot["type"];
    if (!msgType.IsString() || msgType.GetString() != std::string("answer"))
        return AnswerResult::noop;
    const auto& sdp = sdpRoot["sdp"];
    if (!sdp.IsString())
        return AnswerResult::noop;
    return m_session->processSdpAnswer(sdp.GetString());
}

bool Tracker::processMessages()
{
    /* http://rtcweb-wg.github.io/jsep/
     * JSEP's handling of session descriptions is simple and straightforward.
     * Whenever an offer/answer exchange is needed, the initiating side creates an offer
     * by calling a createOffer() API. The application then uses that offer to set up its
     * local config via the setLocalDescription() API. The offer is finally sent off to the
     * remote side over its preferred signaling mechanism (e.g., WebSockets); upon receipt
     * of that offer, the remote party installs it using the setRemoteDescription() API.
     * To complete the offer/answer exchange, the remote party uses the createAnswer() API
     * to generate an appropriate answer, applies it using the setLocalDescription() API,
     * and sends the answer back to the initiator over the signaling channel. When the initiator
     * gets that answer, it installs it using the setRemoteDescription() API, and initial setup
     * is complete. This process can be repeated for additional offer/answer exchanges.
     * */
    AnswerResult result = examineAnswer();
    if (result == AnswerResult::again)
    {
        sendOffer();
        return true;
    }

    NX_VERBOSE(this, "Answer examination result: %1 in stage: %2", (int)result, (int)m_stage);

    m_session->consumer()->startProvidersIfNeeded();

    switch (m_stage)
    {
        case Stage::offer:
            if (result == AnswerResult::failed)
            {
                return false;
            }
            else if (result == AnswerResult::success)
            {
                m_stage = Stage::answerSuccess;
                NX_VERBOSE(this, "Answer succeeded, send ice candidates");
                const auto iceCandidates = m_session->getIceCandidates();
                for (const auto& candidate: iceCandidates)
                    if (candidate.type != IceCandidate::Type::Srflx)
                        sendIce(candidate); //< Send Srflx only after get one from remote peer.
            }
            return true;
        case Stage::answerSuccess:
        case Stage::srflxReceived:
        case Stage::srflxSent:
            return result != AnswerResult::failed;
        default:
            NX_ASSERT(false);
            return false;
    }
}

void Tracker::onIceCandidate(const IceCandidate& candidate)
{
    // Can be called from another thread.
    m_pollable.dispatch(
        [this, candidate]()
        {
            if (m_stage == Stage::srflxReceived || m_stage == Stage::srflxSent)
            {
                sendIce(candidate);
                if (m_stage == Stage::srflxReceived)
                    m_stage = Stage::srflxSent;
            }
            else
            {
                NX_ASSERT(candidate.type == IceCandidate::Type::Srflx);
                m_srflxCandidate = candidate; //< Can't be relay candidate in this stage.
            }
        });
}

void Tracker::onBytesRead(SystemError::ErrorCode errorCode, std::size_t /*bytesTransferred*/)
{
    if (errorCode != SystemError::noError || !processMessages())
    {
        stopUnsafe();
    }
    else
    {
        m_buffer.clear();
        readMessage();
    }
}

} // namespace nx::webrtc
