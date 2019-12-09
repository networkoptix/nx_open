#include "file_streamer.h"

#include <chrono>

#include <nx/streaming/video_data_packet.h>
#include <nx/network/abstract_socket.h>
#include <nx/streaming/config.h> //< for CL_MAX_CHANNELS

#include <nx/vms/testcamera/test_camera_ini.h>
#include <nx/vms/testcamera/packet.h>

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
    const CameraOptions& cameraOptions,
    nx::network::AbstractStreamSocket* socket,
    StreamIndex streamIndex,
    QString filename,
    int channelCount,
    PtsUnloopingContext* ptsUnloopingContext)
    :
    m_logger(logger),
    m_frameLogger(frameLogger),
    m_cameraOptions(cameraOptions),
    m_socket(socket),
    m_streamIndex(streamIndex),
    m_filename(std::move(filename)),
    m_channelCount(channelCount),
    m_ptsUnloopingContext(ptsUnloopingContext)
{
    NX_ASSERT(m_channelCount >= 1);
    NX_ASSERT(m_channelCount <= CL_MAX_CHANNELS);
}

microseconds FileStreamer::framePts(const QnCompressedVideoData* frame) const
{
    if (!NX_ASSERT(frame))
        return microseconds(-1);

    if (ini().warnIfFramePtsDiffersFromTimestamp && frame->timestamp != frame->pts)
    {
        // Log the warning only once for each such frame.
        if (const auto& [it, absent] = m_framesWithDifferentPtsAndTimestamp.insert(frame); absent)
        {
            NX_LOGGER_WARNING(m_logger, "B-frame suspected: timestamp %1, PTS %2.",
                us(frame->timestamp), us(frame->pts));
        }
    }

    if (ini().obtainFramePtsFromTimestampField)
        return microseconds(frame->timestamp);
    else
        return microseconds(frame->pts);
}

bool FileStreamer::streamFrame(const QnCompressedVideoData* frame, int frameIndex) const
{
    try
    {
        if (frameIndex == 0)
            sendMediaContextPacket(frame);

        sendFramePacket(frame);

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

    if (!NX_ASSERT(context.firstFramePts))
        return;

    if (!context.prevPrevPts || !context.prevPts)
    {
        NX_LOGGER_ERROR(m_logger, "Unlooping: The file seems to have less than 3 frames.");
        return;
    }

    if (context.prevPrevPts >= context.prevPts || context.prevPrevPts <= context.firstFramePts)
    {
        NX_LOGGER_ERROR(m_logger, "Unlooping: Bad ptses of file's last 3 frames: %1, %2, %3.",
            us(context.prevPrevPts), us(context.prevPts), us(pts));
        return;
    }

    if (context.prevPrevPts < 0s
        || context.prevPts < 0s
        || context.prevPrevPts >= context.prevPts
        || context.prevPrevPts <= context.firstFramePts)
    {
        NX_LOGGER_ERROR(m_logger, "Unlooping: Bad ptses of file's last 3 frames: %1, %2, %3.",
            us(context.prevPrevPts), us(context.prevPts), us(pts));
        return;
    }

    const microseconds delayBetweenLoops = *context.prevPts - *context.prevPrevPts;
    context.unloopingPeriod = *context.prevPts - *context.firstFramePts + delayBetweenLoops;

    NX_LOGGER_INFO(m_logger, "Unlooping: File period is %1.", us(context.unloopingPeriod));
}

void FileStreamer::obtainRequestedShift() const
{
    if (!NX_ASSERT(m_ptsUnloopingContext))
        return;
    PtsUnloopingContext& context = *m_ptsUnloopingContext;

    context.requestedShift = microseconds(0); //< Makes sure this method will execute only once.

    const std::optional<microseconds> shiftPtsPeriod = (m_streamIndex == StreamIndex::secondary)
        ? m_cameraOptions.shiftPtsSecondaryPeriod
        : m_cameraOptions.shiftPtsPrimaryPeriod;

    if (!m_cameraOptions.shiftPtsFromNow && !shiftPtsPeriod)
        return; //< No shift has been requested.

    const auto now = std::chrono::duration_cast<microseconds>(
        std::chrono::system_clock::now().time_since_epoch());

    if (!NX_ASSERT(!(shiftPtsPeriod && m_cameraOptions.shiftPtsFromNow)))
        return;

    if (m_cameraOptions.shiftPtsFromNow)
        context.requestedShift = now + *m_cameraOptions.shiftPtsFromNow;
    else if (shiftPtsPeriod)
        context.requestedShift = (now / *shiftPtsPeriod - 1) * *shiftPtsPeriod;

    NX_LOGGER_INFO(m_logger, "Unlooping: Shifting unlooped PTSes by %1.",
        us(context.requestedShift));
}

microseconds FileStreamer::processPtsIfNeeded(const microseconds pts) const
{
    const microseconds newPts = unloopAndShiftPtsIfNeeded(pts);

    if (m_cameraOptions.shiftPts)
    {
        if (!NX_ASSERT(!m_cameraOptions.shiftPtsFromNow))
            return newPts;

        return newPts + *m_cameraOptions.shiftPts;
    }

    return newPts;
}

microseconds FileStreamer::unloopAndShiftPtsIfNeeded(const microseconds pts) const
{
    if (!NX_ASSERT(m_ptsUnloopingContext))
        return pts;
    PtsUnloopingContext& context = *m_ptsUnloopingContext;

    if (!m_cameraOptions.unloopPts)
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

        context.unloopingShift += *context.unloopingPeriod;

        NX_LOGGER_DEBUG(m_logger, "Unlooping: Starting new loop with unlooping shift %1.",
            us(context.unloopingShift));
    }
    const microseconds unloopedPts = pts + context.unloopingShift;

    if (!context.requestedShift) //< The very first frame of this file streaming session.
        obtainRequestedShift();

    return unloopedPts + *context.requestedShift;
}

/** @throws SocketError */
void FileStreamer::sendMediaContextPacket(const QnCompressedVideoData* frame) const
{
    if (frame->compressionType == AV_CODEC_ID_NONE || (frame->compressionType > 255))
        throw Error(lm("Codec id %1 not supported by testcamera.").args(frame->compressionType));

    const QByteArray mediaContext = frame->context->serialize();

    QByteArray buffer;

    auto header = packet::makeAppended<packet::Header>(&buffer);
    header->setFlags(packet::Flag::keyFrame | packet::Flag::mediaContext);
    header->setCodecId(frame->compressionType);
    header->setDataSize(mediaContext.size());

    buffer.append(mediaContext);

    send(buffer.constData(), buffer.size(), "media context packet");
}

/** @throws SocketError */
void FileStreamer::sendFramePacket(const QnCompressedVideoData* frame) const
{
    if (frame->compressionType == AV_CODEC_ID_NONE || (frame->compressionType > 255))
        throw Error(lm("Codec id %1 not supported.").args(frame->compressionType));

    if (frame->dataSize() == 0)
        throw Error("Frame with zero size found - not supported.");

    if (frame->dataSize() > 8 * 1024 * 1024)
    {
        throw Error(lm("Frame larger than 8 MB (actual: %1 bytes) found - not supported.")
            .args(frame->dataSize()));
    }

    packet::Flags flags;
    if (frame->flags & AV_PKT_FLAG_KEY)
        flags |= packet::Flag::keyFrame;
    if (m_cameraOptions.includePts && NX_ASSERT(m_ptsUnloopingContext))
        flags |= packet::Flag::ptsIncluded;
    if (m_channelCount > 1)
        flags |= packet::Flag::channelNumberIncluded;

    QByteArray buffer;

    auto header = packet::makeAppended<packet::Header>(&buffer);
    header->setFlags(flags);
    header->setCodecId(frame->compressionType);
    header->setDataSize((int) frame->dataSize());

    QString ptsLogText = "without PTS";
    if (flags & packet::Flag::ptsIncluded)
    {
        const microseconds pts = processPtsIfNeeded(framePts(frame));
        packet::makeAppended<packet::PtsUs>(&buffer, pts.count());
        ptsLogText = lm("with PTS %1").args(us(pts));
    }

    if (flags & packet::Flag::channelNumberIncluded)
        packet::makeAppended<uint8_t>(&buffer, frame->channelNumber);

    m_frameLogger->logFrameIfNeeded(
        lm("Sending frame %1, %2 bytes.").args(ptsLogText, frame->dataSize()),
        m_logger);

    send(buffer.constData(), buffer.size(), "frame header with optional params");

    send(frame->data(), (int) frame->dataSize(), "frame data");
}

} // namespace nx::vms::testcamera
