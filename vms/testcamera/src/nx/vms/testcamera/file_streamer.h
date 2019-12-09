#pragma once

#include <chrono>
#include <set>
#include <optional>

#include <QtCore/QString>
#include <QtCore/QtEndian>

#include <nx/vms/api/types/motion_types.h> //< For StreamIndex.
using nx::vms::api::StreamIndex;

#include "camera_options.h"

namespace nx::network { class AbstractStreamSocket; }
class QnCompressedVideoData;

namespace nx::vms::testcamera {

class Logger;
class FrameLogger;

/**
 * Performs streaming of a single given video file - sends each frame to the socket. A dedicated
 * instance should be created per each streaming iteration of a given file, using the same
 * PtsUnloopingContext instance for all streaming iterations of this file.
 */
class FileStreamer
{
public:
    using microseconds = std::chrono::microseconds;
    using OptionalUs = std::optional<std::chrono::microseconds>;

    struct PtsUnloopingContext
    {
        OptionalUs firstFramePts;
        int loopIndex = 0;
        OptionalUs prevPts;
        OptionalUs prevPrevPts;
        OptionalUs unloopingPeriod;
        microseconds unloopingShift{0}; /**< Caused by the current unlooping iteration. */
        OptionalUs requestedShift; /**< Caused by the explicitly requested shifting; set once */
    };

    /** @param ptsUnloopingContext Must be null if unlooping is not requested. */
    FileStreamer(
        const Logger* logger,
        const FrameLogger* frameLogger,
        const CameraOptions& cameraOptions,
        nx::network::AbstractStreamSocket* socket,
        StreamIndex streamIndex,
        QString filename,
        int channelCount,
        PtsUnloopingContext* ptsUnloopingContext);

    /** Obtain frame PTS from either its `pts` field or `timestamp` field, depending on .ini. */
    microseconds framePts(const QnCompressedVideoData* frame) const;

    /**
     * Must be called sequentially for each frame of the video file. On error, logs the message.
     */
    bool streamFrame(const QnCompressedVideoData* frame, int frameIndex) const;

private:
    void send(const void* data, int size, const QString& dataCaptionForErrorMessage) const;

    void obtainUnloopingPeriod(microseconds pts) const;
    void obtainRequestedShift() const;
    microseconds processPtsIfNeeded(const microseconds pts) const;
    microseconds unloopAndShiftPtsIfNeeded(const microseconds pts) const;
    void sendMediaContextPacket(const QnCompressedVideoData* frame) const;
    void sendFramePacket(const QnCompressedVideoData* frame) const;

private:
    const Logger* const m_logger;
    const FrameLogger* m_frameLogger;
    const CameraOptions m_cameraOptions;
    nx::network::AbstractStreamSocket* const m_socket;
    const StreamIndex m_streamIndex;
    const QString m_filename;
    const int m_channelCount;
    PtsUnloopingContext* const m_ptsUnloopingContext;
    mutable std::set<const QnCompressedVideoData*> m_framesWithDifferentPtsAndTimestamp;
};

} // namespace nx::vms::testcamera
