// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <deque>
#include <map>

#include <nx/media/annexb_to_mp4.h>
#include <nx/network/aio/basic_pollable.h>
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
    enum StreamStatus : int
    {
        wait = http::StatusCode::_continue, //< Don't know result at this moment.
        success = http::StatusCode::ok, //< Success.
        reconnect = http::StatusCode::movedPermanently, //< Only a reconnect can resume the stream.
        forbidden = http::StatusCode::forbidden, //< No right to view this timestamp.
        nodata = http::StatusCode::notFound, //< No provider for this timestamp, can't seek.
        //< The Device changed to a codec the client did not accept in its answer, and no offer
        //< can add one it cannot decode: its stream is over. See Session::switchCodec().
        codecNotSupported = http::StatusCode::unsupportedMediaType,
    };
    using StatusHandler = std::function<void(nx::Uuid deviceId, StreamStatus)>;

    /** Tells if the user of the session may view the Device at this time; DATETIME_NOW is Live. */
    using SeekGuard =
        std::function<bool(const QnVirtualCameraResourcePtr& device, int64_t timestampUs)>;

public:
    Consumer(Session* session);
    virtual ~Consumer();

    /**
     * Makes seek() ask `guard` before it moves a Device, and report `forbidden` if it refuses. The
     * position a session is created with is checked by its creator; a seek changes it afterwards.
     * To be called before the session gets its ICE. Every seek is allowed if not called.
     */
    void setSeekGuard(SeekGuard guard) { m_seekGuard = std::move(guard); }

    void putData(const QnConstAbstractMediaDataPtr& data);

    // API for Streamer.
    void startStream(Streamer* streamer);
    bool handleSrtp(std::vector<uint8_t> buffer);
    void seek(nx::Uuid deviceId, int64_t timestampUs, StatusHandler handler);
    void startProvidersIfNeeded();
    void changeQuality(
        nx::Uuid deviceId,
        nx::vms::api::StreamIndex stream,
        StatusHandler handler);
    void pause(nx::Uuid deviceId, StatusHandler handler);
    void resume(nx::Uuid deviceId, StatusHandler handler);
    void nextFrame(nx::Uuid deviceId, StatusHandler handler);
    void onDataSent(bool success);
    void clearQueueForDevice(nx::Uuid deviceId);
    int64_t lastReportedTimestampUs() { return m_lastReportedTimestampUs; }
    void setSendTimestampInterval(std::chrono::milliseconds interval);

    bool canAcceptData() const;
    std::string idForToStringFromPtr() const;

    /**
     * Runs func in the session AIO thread (Session::aioThread()), the one that owns this consumer,
     * the muxer and the providers. Anything touching that state must go through here, because the
     * ICE does not necessarily live in the same thread -- IceTcp has its own, see startStream().
     */
    void runInMuxingThread(std::function<void()>&& func);

private:
    friend class Ice;

    // Streaming related.
    void processNextData();
    void sendMetadata(const QnConstCompressedMetadataPtr& metadata);
    void handleRtcp(uint8_t* data, int size);
    void stopUnsafe();
    void sendMediaBuffer();

private:
    // Main.
    nx::vms::server::MediaQueue m_mediaQueue;
    // Set from the ICE thread, read in the session thread, see stopUnsafe().
    std::atomic<bool> m_needStop{false};
    nx::network::aio::BasicPollable m_pollable;
    Session* m_session = nullptr;
    Streamer* m_streamer = nullptr;
    int64_t m_lastReportedTimestampUs = AV_NOPTS_VALUE;
    std::chrono::milliseconds m_sendTimestampInterval {1000};
    SeekGuard m_seekGuard;
    bool m_sendInProgress = false;
    // The Archive delegate repeats cannotDecryptMedia for the same packet until it is decrypted,
    // so the last reported packet is kept per device, as a session can stream several devices, to
    // report it once. Reset on seek and on start, to report the packet again.
    std::map<nx::Uuid, int64_t> m_undecryptableTimestampsUs;
    const std::string m_sessionId;
};

} // namespace nx::webrtc
