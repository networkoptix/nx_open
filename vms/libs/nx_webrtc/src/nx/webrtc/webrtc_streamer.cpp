// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "webrtc_streamer.h"

#include <core/resource/camera_resource.h>
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

Streamer::~Streamer()
{
}

bool Streamer::start(Ice* ice, Dtls* dtls)
{
    NX_ASSERT(ice);
    m_ice = ice;
    m_iceType = m_ice->type();
    updateMetrics(1);

    m_session->muxer()->setSrtpEncryptionData(dtls->encryptionData());

    if (!m_session->consumer()->startStream(this))
        return false;

    for (auto& provider: m_session->allProviders())
    {
        m_session->consumer()->seek(
            provider->resource()->getId(),
            provider->getPosition(),
            [this, provider](nx::Uuid deviceId, Consumer::StreamStatus result)
            {
                handleStreamStatus(deviceId, result);
            });
    }
    return true;
}

void Streamer::stop()
{
    updateMetrics(-1);
    m_session->stopAllProviders();
    m_iceType = IceCandidate::Filter::None;
}

bool Streamer::onSrtp(std::vector<uint8_t> buffer)
{
    return m_session->consumer()->handleSrtp(std::move(buffer));
}

static nx::Uuid parseDeviceId(const rapidjson::Value& obj)
{
    if (!obj.IsObject())
        return {};

    auto it = obj.FindMember("deviceId");
    if (it == obj.MemberEnd() || !it->value.IsString())
        return {};

    return nx::Uuid::fromStringSafe(it->value.GetString());
}

static int64_t parseSeekMs(const rapidjson::Value& obj)
{
    auto it = obj.FindMember("seek");
    if (it == obj.MemberEnd())
        return -1;

    const auto& v = it->value;
    if (v.IsDouble())
        return static_cast<int64_t>(v.GetDouble());
    if (v.IsInt64())
        return v.GetInt64();
    if (v.IsInt())
        return v.GetInt();
    if (v.IsString())
    {
        auto strValue = v.GetString();
        if (strValue == 0 || strValue[0] == 0)
            return -1;
        return std::atoll(strValue);
    }

    return -1;
}

static nx::vms::api::StreamIndex parseStreamIndex(const rapidjson::Value& obj)
{
    auto it = obj.FindMember("stream");
    if (it == obj.MemberEnd())
        return nx::vms::api::StreamIndex::primary;

    const auto& v = it->value;
    if ((v.IsString() && std::string{ v.GetString() } == "1")
        || (v.IsInt() && v.GetInt() == 1)
        || (v.IsInt64() && v.GetInt64() == 1))
    {
        return nx::vms::api::StreamIndex::secondary;
    }

    return nx::vms::api::StreamIndex::primary;
}

void Streamer::onDataChannelString(const std::string& data, int /*streamId*/)
{
    rapidjson::Document d;
    d.Parse(data.data());
    if (!d.IsObject())
        return;

    if (auto it = d.FindMember("add"); it != d.MemberEnd())
    {
        const auto& addObj = it->value;
        if (!addObj.IsObject())
            return;

        const nx::Uuid newDeviceId = parseDeviceId(addObj);
        if (newDeviceId.isNull())
            return;

        const int64_t seekMs = parseSeekMs(addObj);
        std::optional<std::chrono::milliseconds> positionMs;
        if (seekMs >= 0)
            positionMs = std::chrono::milliseconds(seekMs);

        const nx::vms::api::StreamIndex newStream = parseStreamIndex(addObj);

        m_session->createProvider(
            newDeviceId,
            positionMs,
            newStream,
            std::nullopt /*speedOpt*/);
        return;
    }

    nx::Uuid deviceId = parseDeviceId(d);

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
            deviceId,
            timestampUs,
            [this](nx::Uuid deviceId, Consumer::StreamStatus result)
            {
                handleStreamStatus(deviceId, result);
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
            deviceId,
            newStream,
            [this](nx::Uuid deviceId, Consumer::StreamStatus result)
            {
                handleStreamStatus(deviceId, result);
            });
    }
    else if (d.HasMember("pause"))
    {
        m_session->consumer()->pause(
            deviceId,
            [this](nx::Uuid deviceId, Consumer::StreamStatus result)
            {
                handleStreamStatus(deviceId, result);
            });
    }
    else if (d.HasMember("resume"))
    {
        m_session->consumer()->resume(
            deviceId,
            [this](nx::Uuid deviceId, Consumer::StreamStatus result)
            {
                handleStreamStatus(deviceId, result);
            });
    }
    else if (d.HasMember("nextFrame"))
    {
        m_session->consumer()->nextFrame(
            deviceId,
            [this](nx::Uuid deviceId, Consumer::StreamStatus result)
            {
                handleStreamStatus(deviceId, result);
            });
    }
}

void Streamer::onDataChannelBinary(const std::string& data, int streamId)
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
    // TODO: this function doesn't support multi camera streaming properly. It is needed to refactor it.
    rapidjson::Document message(rapidjson::kObjectType);
    message.AddMember("timestampMs", (int64_t) timestampUs / 1000, message.GetAllocator());
    message.AddMember("rtpTimestamp", rtpTimestamp, message.GetAllocator());
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    message.Accept(writer);
    std::string data{buffer.GetString(), buffer.GetSize()};

    ice()->writeDataChannelString(data);
}

void Streamer::handleStreamStatus(
    nx::Uuid deviceId,
    Consumer::StreamStatus result)
{
    auto provider = m_session->provider(deviceId);
    if (!provider)
        return;

    rapidjson::Document message(rapidjson::kObjectType);
    auto positionMs = provider->isLiveMode()
        ? -1.0
        : provider->getPosition() / 1000;

    if (result == Consumer::StreamStatus::reconnect) //< If reconnect send timestamp to reconnect.
        positionMs = m_session->muxer()->getLastTimestampMs();

    message.AddMember(
        "deviceId",
        provider->resource()->getId().toSimpleStdString(),
        message.GetAllocator());
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
