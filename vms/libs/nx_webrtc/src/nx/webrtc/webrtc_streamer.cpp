// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "webrtc_streamer.h"

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <nx/metric/metrics_storage.h>

#include "webrtc_session.h"
#include "webrtc_session_pool.h"

namespace nx::webrtc {

Streamer::Streamer(Session* session): m_session(session)
{
}

/*virtual*/ Streamer::~Streamer()
{
}

/*virtual*/ bool Streamer::start(Ice* ice, Dtls* dtls)
{
    NX_ASSERT(ice);
    m_ice = ice;
    m_iceType = m_ice->type();
    updateMetrics(1);

    m_session->muxer()->setSrtpEncryptionData(dtls->encryptionData());

    if (!m_session->consumer()->startStream(this))
        return false;

    m_session->consumer()->seek(
        m_session->provider()->getPosition(),
        [this](Consumer::StreamStatus result)
        {
            handleStreamStatus(result);
        });
    return true;
}

/*virtual*/ void Streamer::stop()
{
    updateMetrics(-1);
    m_session->provider()->stop();
    m_iceType = IceCandidate::Filter::None;
}

/*virtual*/ bool Streamer::onSrtp(std::vector<uint8_t> buffer)
{
    return m_session->consumer()->handleSrtp(std::move(buffer));
}

/*virtual*/ void Streamer::onDataChannelString(const std::string& data, int /*streamId*/)
{
    rapidjson::Document d;
    d.Parse(data.data());
    if (!d.IsObject())
        return;
    if (d.HasMember("seek"))
    {
        int64_t timestamp = 0;
        const auto& seekValue = d["seek"];
        if (seekValue.IsDouble())
            timestamp = seekValue.GetDouble();
        else if (seekValue.IsString())
            timestamp = std::atoll(seekValue.GetString());
        // Assume that timestamp is in milliseconds since epoch.
        int64_t timestampUs = (timestamp < 0 ? DATETIME_NOW : (int64_t) timestamp * 1000);
        m_session->consumer()->seek(
            timestampUs,
            [this](Consumer::StreamStatus result)
            {
                handleStreamStatus(result);
            });
    }
    else if (d.HasMember("stream"))
    {
        nx::vms::api::StreamIndex newStream = nx::vms::api::StreamIndex::primary;
        const auto& streamValue = d["stream"];
        if ((streamValue.IsString() && std::string{streamValue.GetString()} == "1")
            || (streamValue.IsInt() && streamValue.GetInt() == 1))
            newStream = nx::vms::api::StreamIndex::secondary;
        m_session->consumer()->changeQuality(
            newStream,
            [this](Consumer::StreamStatus result)
            {
                handleStreamStatus(result);
            });
    }
    else if (d.HasMember("pause"))
    {
        m_session->consumer()->pause(
            [this](Consumer::StreamStatus result)
            {
                handleStreamStatus(result);
            });
    }
    else if (d.HasMember("resume"))
    {
        m_session->consumer()->resume(
            [this](Consumer::StreamStatus result)
            {
                handleStreamStatus(result);
            });
    }
    else if (d.HasMember("nextFrame"))
    {
        m_session->consumer()->nextFrame(
            [this](Consumer::StreamStatus result)
            {
                handleStreamStatus(result);
            });
    }
}

/*virtual*/ void Streamer::onDataChannelBinary(const std::string& data, int streamId)
{
    onDataChannelString(data, streamId);
}

Ice* Streamer::ice()
{
    NX_ASSERT(m_ice);
    return m_ice;
}

void Streamer::sendTimestamp(int64_t timestampUs, uint32_t rtpTimestamp)
{
    rapidjson::Document message(rapidjson::kObjectType);
    message.AddMember("timestampMs", (int64_t) timestampUs / 1000, message.GetAllocator());
    message.AddMember("rtpTimestamp", rtpTimestamp, message.GetAllocator());
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    message.Accept(writer);
    std::string data{buffer.GetString(), buffer.GetSize()};

    ice()->writeDataChannelString(data);
}

void Streamer::handleStreamStatus(Consumer::StreamStatus result)
{
    rapidjson::Document message(rapidjson::kObjectType);
    auto positionMs = m_session->provider()->isLiveMode()
        ? -1.0
        : m_session->provider()->getPosition() / 1000;

    if (result == Consumer::StreamStatus::reconnect) //< If reconnect send timestamp to reconnect.
        positionMs = m_session->muxer()->getLastTimestampMs();

    message.AddMember(
        "timestampMs",
        positionMs,
        message.GetAllocator());
    message.AddMember("status", result, message.GetAllocator());
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    message.Accept(writer);
    NX_DEBUG(this, "Write stream status: %1", buffer.GetString());

    std::string data{buffer.GetString(), buffer.GetSize()};

    ice()->writeDataChannelString(data);
}

void Streamer::updateMetrics(int value)
{
    switch (m_iceType)
    {
        case IceCandidate::Filter::TcpHost:
            if (value > 0)
                m_session->metrics()->webrtc().totalTcpConnections() += value;
            m_session->metrics()->webrtc().activeTcpConnections() += value;
            break;
        case IceCandidate::Filter::UdpHost:
            if (value > 0)
                m_session->metrics()->webrtc().totalUdpConnections() += value;
            m_session->metrics()->webrtc().activeUdpConnections() += value;
            break;
        case IceCandidate::Filter::UdpSrflx:
            if (value > 0)
                m_session->metrics()->webrtc().totalSrflxConnections() += value;
            m_session->metrics()->webrtc().activeSrflxConnections() += value;
            break;
        case IceCandidate::Filter::UdpRelay:
            if (value > 0)
                m_session->metrics()->webrtc().totalRelayConnections() += value;
            m_session->metrics()->webrtc().activeRelayConnections() += value;
            break;
        case IceCandidate::Filter::None:
            break;
        default:
            NX_ASSERT(false, "Unexpected ice type: %1", m_iceType);
            break;
    }
}

} // namespace nx::webrtc
