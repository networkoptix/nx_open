// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <deque>

#include <nx/media/annexb_to_mp4.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/aio/timer.h>
#include <nx/network/http/http_types.h>
#include <nx/rtp/rtcp.h>
#include <nx/rtp/rtp.h>
#include <nx/vms/api/types/motion_types.h>

#include "abstract_camera_data_provider.h"

namespace nx::webrtc {

class Session;
class Streamer;

using namespace nx::network;

class NX_WEBRTC_API Consumer
{
public:

    enum StreamStatus: int
    {
        wait = http::StatusCode::_continue, //< Don't know result at this moment.
        success = http::StatusCode::ok, //< Success.
        reconnect = http::StatusCode::movedPermanently, //< Stream is possible only after reconnect.
        nodata = http::StatusCode::notFound, //< No provider for this timestamp, can't seek.
    };
    using StatusHandler = std::function<void(nx::Uuid deviceId, StreamStatus)>;

public:
    Consumer(Session* session);
    virtual ~Consumer();

    void putData(const QnConstAbstractMediaDataPtr& data);

    // API for Streamer.
    bool startStream(Streamer* streamer);
    bool handleSrtp(std::vector<uint8_t> buffer);
    void seek(nx::Uuid deviceId, int64_t timestampUs, StatusHandler&& handler);
    void startProvidersIfNeeded();
    void changeQuality(
        nx::Uuid deviceId,
        nx::vms::api::StreamIndex stream,
        StatusHandler&& handler);
    void pause(nx::Uuid deviceId, StatusHandler&& handler);
    void resume(nx::Uuid deviceId, StatusHandler&& handler);
    void nextFrame(nx::Uuid deviceId, StatusHandler&& handler);
    void onDataSent(bool success);
    int64_t lastReportedTimestampUs() { return m_lastReportedTimestampUs; }
    void setSendTimestampInterval(std::chrono::milliseconds interval);
    void setEnableMetadata(bool enableMetadata);

private:
    friend class Ice;

    // Streaming related.
    void processNextData();
    void sendMetadata(const QnConstCompressedMetadataPtr& metadata);
    void handleRtcp(uint8_t* data, int size);
    void runInMuxingThread(std::function<void()>&& func);
    void stopUnsafe();
    void sendMediaBuffer();
private:
    // Main.
    nx::vms::server::MediaQueue m_mediaQueue;
    bool m_needStop = false;
    nx::network::aio::BasicPollable m_pollable;
    Session* m_session = nullptr;
    Streamer* m_streamer = nullptr;
    int64_t m_lastReportedTimestampUs = AV_NOPTS_VALUE;
    std::chrono::milliseconds m_sendTimestampInterval {1000};
    bool m_enableMetadata = false;
    bool m_sendInProgress = false;
};

} // namespace nx::webrtc
