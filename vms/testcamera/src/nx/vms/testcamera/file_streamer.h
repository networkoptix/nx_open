#pragma once

#include <chrono>
#include <set>

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

    struct PtsUnloopingContext
    {
        microseconds firstFramePts{-1};
        int loopIndex = 0;
        microseconds prevPts{-1};
        microseconds prevPrevPts{-1};
        microseconds unloopingPeriod{-1};
        microseconds unloopingShift{0}; /**< Shift caused by the current unlooping iteration. */
        microseconds shiftingShift{0}; /**< Shift caused by the requested shifting; set once. */
    };

    /** @param ptsUnloopingContext Must be null if unlooping is not requested. */
    FileStreamer(
        const Logger* logger,
        const FrameLogger* frameLogger,
        const CameraOptions& testCameraOptions,
        nx::network::AbstractStreamSocket* socket,
        StreamIndex streamIndex,
        QString filename,
        PtsUnloopingContext* ptsUnloopingContext);

    /** Obtain frame PTS from either its `pts` field or `timestamp` field, depending on .ini. */
    microseconds framePts(const QnCompressedVideoData* frame) const;

    /**
     * Must be called sequentially for each frame of the video file. On error, logs the message.
     */
    bool streamFrame(const QnCompressedVideoData* frame, int frameIndex) const;

private:
    template<class Value>
    void sendAsBigEndian(Value value, const QString& dataCaptionForErrorMessage) const
    {
        const auto valueAsBigEndian = qToBigEndian(value);
        send(&valueAsBigEndian, sizeof(valueAsBigEndian), dataCaptionForErrorMessage);
    }

    // Workaround for Qt deficiency: on Linux, int64_t is long, and there is no qToBigEndian(long).
    void sendAsBigEndian(int64_t value, const QString& dataCaptionForErrorMessage) const
    {
        const auto valueAsBigEndian = qToBigEndian((qint64) value);
        send(&valueAsBigEndian, sizeof(valueAsBigEndian), dataCaptionForErrorMessage);
    }

    void send(const void* data, int size, const QString& dataCaptionForErrorMessage) const;

    void obtainUnloopingPeriod(microseconds pts) const;
    microseconds unloopAndShiftPtsIfNeeded(const microseconds pts) const;
    void sendMediaContextWithCodecAndFlags(const QnCompressedVideoData* frame) const;
    void sendFrame(const QnCompressedVideoData* frame) const;

private:
    const Logger* const m_logger;
    const FrameLogger* m_frameLogger;
    const CameraOptions m_testCameraOptions;
    nx::network::AbstractStreamSocket* const m_socket;
    const StreamIndex m_streamIndex;
    const QString m_filename;
    PtsUnloopingContext* const m_ptsUnloopingContext;
    mutable std::set<const QnCompressedVideoData*> m_framesWithDifferentPtsAndTimestamp;
};

} // namespace nx::vms::testcamera
