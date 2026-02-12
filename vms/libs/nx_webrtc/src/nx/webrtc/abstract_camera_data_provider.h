// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <audit/audit_manager_fwd.h>
#include <nx/media/annexb_to_mp4.h>
#include <nx/streaming/abstract_archive_stream_reader.h>
#include <nx/streaming/abstract_data_consumer.h>
#include <core/dataconsumer/abstract_data_receptor.h>
#include <transcoding/media_transcoder.h>
#include "media_queue.h"

namespace nx::metric { struct Storage; }

namespace nx::webrtc {

class NX_WEBRTC_API AbstractCameraDataProvider:
    public QnAbstractMediaDataReceptor
{
public:

    // Data handler, will put nullptr on EOF.
    using OnDataHandler =
        std::function<void (const QnConstAbstractMediaDataPtr& packet)>;

    int64_t getPosition() const { return m_positionUs; }
    void setPosition(int64_t positionUs) { m_positionUs = (positionUs >= 0 ? positionUs : DATETIME_NOW); }

    bool isInitialized() const { return m_initialized;}
    void setInitialized(bool value) { m_initialized = value; }
    virtual bool isStarted() const = 0;

public: // Initialize.
    virtual void subscribe(const OnDataHandler& handler) = 0;

    void setAuditHandle(const AuditHandle& handle) { m_auditHandle = handle; }

    virtual bool addVideoTranscoder(
        QnFfmpegVideoTranscoder::Config config,
        QnLegacyTranscodingSettings transcodingSettings) = 0;
    virtual bool addAudioTranscoder(AVCodecID codec) = 0;
    virtual bool isTranscodingEnabled(QnAbstractMediaData::DataType dataType) const = 0;

    virtual AVCodecParameters* getVideoCodecParameters() const = 0;
    virtual AVCodecParameters* getAudioCodecParameters() const = 0;

    virtual QnVirtualCameraResourcePtr resource() = 0;

public: // Playback.
    virtual bool isLiveMode() const = 0;
    virtual bool paused() const = 0;
    virtual bool play(int64_t positionUs, bool useAudioLastGop) = 0;
    virtual bool changeQuality(nx::vms::api::StreamIndex stream) = 0;
    virtual bool pause() = 0;
    virtual bool resume() = 0;
    virtual bool nextFrame() = 0;
    virtual void stop() = 0;

protected:
    int64_t m_positionUs = DATETIME_NOW;
    AuditHandle m_auditHandle;
    bool m_initialized = false;
};

using AbstractCameraDataProviderPtr = std::shared_ptr<AbstractCameraDataProvider>;
using createDataProviderFactory = nx::MoveOnlyFunc<AbstractCameraDataProviderPtr(
    nx::Uuid /*deviceId*/,
    std::optional<std::chrono::milliseconds> /*positionMs*/,
    nx::vms::api::StreamIndex /*stream*/,
    std::optional<float> /*speedOpt*/)>;

} // namespace nx::webrtc
