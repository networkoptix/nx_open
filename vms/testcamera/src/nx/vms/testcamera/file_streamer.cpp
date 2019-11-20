#include "file_streamer.h"

#include <chrono>

#include <nx/streaming/video_data_packet.h>
#include <nx/network/abstract_socket.h>
#include <core/resource/test_camera_ini.h>

#include "logger.h"
#include "frame_logger.h"

namespace nx::vms::testcamera {

using std::chrono::microseconds;
using namespace std::chrono_literals;

namespace {

class Error: public std::exception
{
public:
    Error(const QString& what): m_what(what.toStdString()) {}
    virtual const char* what() const noexcept override { return m_what.c_str(); }

private:
    const std::string m_what;
};


class SocketError: public Error
{
public:
    SocketError(const QString& dataCaption, const QString& errorMessage, int bytesToSend):
        Error(lm("Unable to send %1 (%2 bytes): %3").args(
            dataCaption, bytesToSend, errorMessage))
    {
    }
};

} // namespace

FileStreamer::FileStreamer(
    const Logger* logger,
    const FrameLogger* frameLogger,
    const CameraOptions& testCameraOptions,
    nx::network::AbstractStreamSocket* socket,
    StreamIndex streamIndex,
    QString filename,
    PtsUnloopingContext* ptsUnloopingContext)
    :
    m_logger(logger),
    m_frameLogger(frameLogger),
    m_testCameraOptions(testCameraOptions),
    m_socket(socket),
    m_streamIndex(streamIndex),
    m_filename(std::move(filename)),
    m_ptsUnloopingContext(ptsUnloopingContext)
{
}

microseconds FileStreamer::framePts(const QnCompressedVideoData* frame) const
{
    if (!NX_ASSERT(frame))
        return microseconds(-1);

    if (testCameraIni().warnIfFramePtsDiffersFromTimestamp && frame->timestamp != frame->pts)
    {
        // Log the warning only once for each such frame.
        if (const auto& [it, absent] = m_framesWithDifferentPtsAndTimestamp.insert(frame); absent)
        {
            NX_LOGGER_WARNING(m_logger, "B-frame suspected: timestamp %1, pts %2.",
                us(frame->timestamp), us(frame->pts));
        }
    }

    if (testCameraIni().obtainFramePtsFromTimestampField)
        return microseconds(frame->timestamp);
    else
        return microseconds(frame->pts);
}

bool FileStreamer::streamFrame(const QnCompressedVideoData* frame, int frameIndex) const
{
    try
    {
        if (frameIndex == 0)
            sendMediaContextWithCodecAndFlags(frame);

        sendFrame(frame);

        if (m_ptsUnloopingContext)
        {
            const auto pts = framePts(frame);

            if (frameIndex == 0)
                m_ptsUnloopingContext->firstFramePts = pts;

            m_ptsUnloopingContext->prevPrevPts = m_ptsUnloopingContext->prevPts;
            m_ptsUnloopingContext->prevPts = pts;
        }
    }
    catch (const Error& e)
    {
        NX_LOGGER_ERROR(m_logger, e.what());
        return false;
    }
    return true;
}

/** @throws SocketError */
void FileStreamer::send(
    const void* data, int size, const QString& dataCaptionForErrorMessage) const
{
    int bytesSent = 0;
    int bytesSentTotal = 0;
    while (bytesSentTotal < size)
    {
        const int bytesToSend = size - bytesSentTotal;
        bytesSent = m_socket->send((const char*) data + bytesSentTotal, bytesToSend);
        if (bytesSent <= 0)
        {
            const QString errorMessage =
                (bytesSent == 0) ? "Connection closed." : SystemError::getLastOSErrorText();
            throw SocketError(dataCaptionForErrorMessage, errorMessage, bytesToSend);
        }
        bytesSentTotal += bytesSent;
    }
    NX_ASSERT(bytesSentTotal == size);
}

void FileStreamer::obtainUnloopingPeriod(microseconds pts) const
{
    if (!NX_ASSERT(m_ptsUnloopingContext))
        return;
    PtsUnloopingContext& context = *m_ptsUnloopingContext;

    if (context.prevPrevPts < 0s
        || context.prevPts < 0s
        || context.prevPrevPts >= context.prevPts
        || context.prevPrevPts <= context.firstFramePts)
    {
        NX_LOGGER_ERROR(m_logger, "Unlooping: Bad ptses of file's last 3 frames: %1, %2, %3.",
            us(context.prevPrevPts), us(context.prevPts), us(pts));
        return;
    }

    const microseconds delayBetweenLoops = context.prevPts - context.prevPrevPts;
    context.unloopingPeriod = context.prevPts - context.firstFramePts + delayBetweenLoops;

    NX_LOGGER_INFO(m_logger, "Unlooping: File period is %1.", us(context.unloopingPeriod));
}

microseconds FileStreamer::unloopAndShiftPtsIfNeeded(const microseconds pts) const
{
    if (!NX_ASSERT(m_ptsUnloopingContext))
        return pts;
    PtsUnloopingContext& context = *m_ptsUnloopingContext;

    if (!m_testCameraOptions.unloopPts)
        return pts;

    if (pts < 0s)
    {
        NX_LOGGER_ERROR(m_logger, "Unlooping: Pts is negative.");
        return pts;
    }

    if (pts < context.prevPts) //< The previous frame was the last.
    {
        ++context.loopIndex;

        if (context.loopIndex == 1)
            obtainUnloopingPeriod(pts);

        context.unloopingShift += context.unloopingPeriod;

        NX_LOGGER_VERBOSE(m_logger, "Unlooping: Starting new loop with unlooping shift %1.",
            us(context.unloopingShift));
    }
    const microseconds unloopedPts = pts + context.unloopingShift;

    const microseconds shiftPeriod{(m_streamIndex == StreamIndex::secondary)
        ? m_testCameraOptions.shiftPtsSecondaryPeriodUs
        : m_testCameraOptions.shiftPtsPrimaryPeriodUs};

    if (shiftPeriod < 0s) //< Shifting was not requested in the options.
        return unloopedPts;

    if (context.shiftingShift == 0s) //< The very first frame of this file streaming session.
    {
        const auto now = std::chrono::system_clock::now().time_since_epoch();

        context.shiftingShift = (now / shiftPeriod - 1) * shiftPeriod;

        NX_LOGGER_INFO(m_logger, "Unlooping: Shifting unlooped ptses by %1.",
            us(context.shiftingShift));
    }

    return unloopedPts + context.shiftingShift;
}

/** @throws SocketError */
void FileStreamer::sendMediaContextWithCodecAndFlags(const QnCompressedVideoData* frame) const
{
    const quint16 codecAndFlags =
        frame->compressionType
        | ((/*isKeyFrame*/ 1 << 7) << 8)
        | ((/*isCodecContext*/ 1 << 6) << 8);
    sendAsBigEndian(codecAndFlags, "media context codec and flags");

    const QByteArray mediaContext = frame->context->serialize();
    sendAsBigEndian((int32_t) mediaContext.size(), "media context size");
    send(mediaContext.data(), mediaContext.size(), "media context data");
}

/** @throws SocketError */
void FileStreamer::sendFrame(const QnCompressedVideoData* frame) const
{
    uint16_t codecAndFlags = frame->compressionType;

    if (frame->compressionType == 0
        || (frame->compressionType >= (1 << 5) && frame->compressionType != AV_CODEC_ID_H265))
    {
        throw Error(lm("Codec id %1 not supported by testcamera.").args(frame->compressionType));
    }

    if (frame->flags & AV_PKT_FLAG_KEY)
        codecAndFlags |= (/*isKeyFrame*/ 1 << 7) << 8;

    if (m_testCameraOptions.includePts)
        codecAndFlags |= (/*isPtsIncluded*/ 1 << 5) << 8;

    sendAsBigEndian(codecAndFlags, "frame codec and flags");

    sendAsBigEndian((uint32_t) frame->dataSize(), "frame data size");

    QString ptsLogText = "without pts";
    if (m_testCameraOptions.includePts && NX_ASSERT(m_ptsUnloopingContext))
    {
        const microseconds pts = unloopAndShiftPtsIfNeeded(framePts(frame));
        ptsLogText = lm("with pts %1").args(us(pts));
        sendAsBigEndian((int64_t) pts.count(), "pts");
    }

    m_frameLogger->logFrameIfNeeded(
        lm("Sending frame %1, %2 bytes.").args(ptsLogText, frame->dataSize()),
        m_logger);

    send(frame->data(), (int) frame->dataSize(), "frame data");
}

} // namespace nx::vms::testcamera
