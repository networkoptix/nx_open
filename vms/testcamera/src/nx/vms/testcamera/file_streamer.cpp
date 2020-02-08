#include "file_streamer.h"

#include <chrono>

#include <nx/utils/switch.h>
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

/** Used to terminate streaming, either due to an error, or for any other reason. */
class Exception: public std::exception
{
public:
    enum class Kind { error, finishStreaming };

    Exception(Kind kind, const QString& what): m_kind(kind), m_what(what.toStdString()) {}
    virtual const char* what() const noexcept override { return m_what.c_str(); }
    Kind kind() const { return m_kind; }

private:
    const Kind m_kind;
    const std::string m_what;
};

class SocketError: public Exception
{
public:
    SocketError(const QString& dataCaption, const QString& errorMessage, int bytesToSend):
        Exception(Kind::error, lm("Unable to send %1 (%2 bytes): %3").args(
            dataCaption, bytesToSend, errorMessage))
    {
    }
};

/** The particular data value to be sent is not supported by testcamera. */
class UnsupportedData: public Exception
{
public:
    UnsupportedData(const QString& message):
        Exception(Kind::error, "Unsupported data found in the video file: " + message) {}
};

/** Server has shut down the socket - this is not an error, but the streaming has to stop. */
class SocketWasShutDown: public Exception
{
public:
    SocketWasShutDown():
        Exception(Kind::finishStreaming, "Connection was closed by the Server.")
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
    int channelCount,
    PtsUnloopingContext* ptsUnloopingContext)
    :
    m_logger(logger),
    m_frameLogger(frameLogger),
    m_cameraOptions(cameraOptions),
    m_socket(socket),
    m_streamIndex(streamIndex),
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
    catch (const Exception& e)
    {
        const nx::utils::log::Level logLevel =
            nx::utils::switch_(e.kind(),
                Exception::Kind::error, [] { return nx::utils::log::Level::error; },
                Exception::Kind::finishStreaming, [] { return nx::utils::log::Level::warning; }
            );
        NX_LOGGER_LOG(m_logger, logLevel, e.what());
        return false;
    }
    return true;
}

static std::pair<SystemError::ErrorCode, QString /*errorMessage*/> getLastOsError()
{
    const auto errorCode = SystemError::getLastOSErrorCode();
    return {errorCode, lm("%1 (OS code %2).").args(SystemError::getLastOSErrorText(), errorCode)};
}

/** @throws Exception */
void FileStreamer::send(
    const void* data, int size, const QString& dataCaptionForErrorMessage) const
{
    int sendResult = 0;
    int bytesSentTotal = 0;
    while (bytesSentTotal < size)
    {
        const int bytesToSend = size - bytesSentTotal;

        sendResult = m_socket->send((const char*) data + bytesSentTotal, bytesToSend);

        if (sendResult == 0)
            throw SocketError(dataCaptionForErrorMessage, "Data was not accepted.", bytesToSend);

        if (sendResult < 0)
        {
            const auto [errorCode, errorMessage] = getLastOsError();
            NX_LOGGER_DEBUG(m_logger, "Failed to send %1: send() returned %2: %3",
                dataCaptionForErrorMessage, sendResult, errorMessage);

            if (errorCode == SystemError::connectionAbort)
                throw SocketWasShutDown();
            throw SocketError(dataCaptionForErrorMessage, errorMessage, bytesToSend);
        }

        bytesSentTotal += sendResult;
    }

    NX_ASSERT(bytesSentTotal == size);
}

void FileStreamer::obtainUnloopingPeriod(microseconds pts) const
{
    if (!NX_ASSERT(m_ptsUnloopingContext) || !NX_ASSERT(m_ptsUnloopingContext->firstFramePts))
        return;

    if (!m_ptsUnloopingContext->prevPrevPts || !m_ptsUnloopingContext->prevPts)
    {
        NX_LOGGER_ERROR(m_logger, "Unlooping: The file seems to have less than 3 frames.");
        return;
    }

    if (m_ptsUnloopingContext->prevPrevPts >= m_ptsUnloopingContext->prevPts
        || m_ptsUnloopingContext->prevPrevPts <= m_ptsUnloopingContext->firstFramePts)
    {
        NX_LOGGER_ERROR(m_logger, "Unlooping: Bad ptses of file's last 3 frames: %1, %2, %3.",
            us(m_ptsUnloopingContext->prevPrevPts), us(m_ptsUnloopingContext->prevPts), us(pts));
        return;
    }

    if (m_ptsUnloopingContext->prevPrevPts < 0s
        || m_ptsUnloopingContext->prevPts < 0s
        || m_ptsUnloopingContext->prevPrevPts >= m_ptsUnloopingContext->prevPts
        || m_ptsUnloopingContext->prevPrevPts <= m_ptsUnloopingContext->firstFramePts)
    {
        NX_LOGGER_ERROR(m_logger, "Unlooping: Bad ptses of file's last 3 frames: %1, %2, %3.",
            us(m_ptsUnloopingContext->prevPrevPts), us(m_ptsUnloopingContext->prevPts), us(pts));
        return;
    }

    const microseconds delayBetweenLoops =
        *m_ptsUnloopingContext->prevPts - *m_ptsUnloopingContext->prevPrevPts;
    m_ptsUnloopingContext->unloopingPeriod =
        *m_ptsUnloopingContext->prevPts - *m_ptsUnloopingContext->firstFramePts
        + delayBetweenLoops;

    NX_LOGGER_INFO(m_logger, "Unlooping: File period is %1.",
        us(m_ptsUnloopingContext->unloopingPeriod));
}

void FileStreamer::obtainRequestedShift() const
{
    if (!NX_ASSERT(m_ptsUnloopingContext))
        return;

    // Make sure this method executes only once for the particular PtsUnloopingContext.
    m_ptsUnloopingContext->requestedShift = microseconds(0);

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
        m_ptsUnloopingContext->requestedShift = now + *m_cameraOptions.shiftPtsFromNow;
    else if (shiftPtsPeriod)
        m_ptsUnloopingContext->requestedShift = (now / *shiftPtsPeriod - 1) * *shiftPtsPeriod;

    NX_LOGGER_INFO(m_logger, "Unlooping: Shifting unlooped PTSes by %1.",
        us(m_ptsUnloopingContext->requestedShift));
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
    if (!m_cameraOptions.unloopPts || !NX_ASSERT(m_ptsUnloopingContext))
        return pts;

    if (pts < 0s)
    {
        NX_LOGGER_ERROR(m_logger, "Unlooping: Pts is negative.");
        return pts;
    }

    if (pts < m_ptsUnloopingContext->prevPts) //< The previous frame was the last.
    {
        ++m_ptsUnloopingContext->loopIndex;

        if (m_ptsUnloopingContext->loopIndex == 1)
            obtainUnloopingPeriod(pts);

        m_ptsUnloopingContext->unloopingShift += *m_ptsUnloopingContext->unloopingPeriod;

        NX_LOGGER_DEBUG(m_logger, "Unlooping: Starting new loop with unlooping shift %1.",
            us(m_ptsUnloopingContext->unloopingShift));
    }
    const microseconds unloopedPts = pts + m_ptsUnloopingContext->unloopingShift;

    if (!m_ptsUnloopingContext->requestedShift)
    {
        // The very first frame of this file streaming session.
        obtainRequestedShift();
    }

    return unloopedPts + *m_ptsUnloopingContext->requestedShift;
}

/** @throws Exception */
void FileStreamer::sendMediaContextPacket(const QnCompressedVideoData* frame) const
{
    if (frame->compressionType == AV_CODEC_ID_NONE || (frame->compressionType > 255))
        throw UnsupportedData(lm("Codec id %1.").args(frame->compressionType));

    const QByteArray mediaContext = frame->context->serialize();

    QByteArray buffer;

    auto header = packet::makeAppended<packet::Header>(&buffer);
    header->setFlags(packet::Flag::keyFrame | packet::Flag::mediaContext);
    header->setCodecId(frame->compressionType);
    header->setDataSize(mediaContext.size());

    buffer.append(mediaContext);

    send(buffer.constData(), buffer.size(), "media context packet");
}

/** @throws Exception */
void FileStreamer::sendFramePacket(const QnCompressedVideoData* frame) const
{
    if (frame->compressionType == AV_CODEC_ID_NONE || (frame->compressionType > 255))
        throw UnsupportedData(lm("Codec id %1.").args(frame->compressionType));

    if (frame->dataSize() == 0)
        throw UnsupportedData("Frame with zero size.");

    if (frame->dataSize() > 8 * 1024 * 1024)
    {
        throw UnsupportedData(lm("Frame larger than 8 MB - actually %1 bytes.")
            .args(frame->dataSize()));
    }

    packet::Flags flags;
    if (frame->flags & AV_PKT_FLAG_KEY)
        flags |= packet::Flag::keyFrame;
    if (m_cameraOptions.includePts)
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
