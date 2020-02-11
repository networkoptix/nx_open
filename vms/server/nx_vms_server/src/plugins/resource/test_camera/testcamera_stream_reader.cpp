#include "testcamera_stream_reader.h"
#if defined(ENABLE_TEST_CAMERA)

#include <nx/utils/log/log.h>

#include <nx/streaming/video_data_packet.h>
#include <nx/streaming/basic_media_context.h>
#include <nx/vms/testcamera/packet.h>
#include "testcamera_resource.h"
#include "utils/common/synctime.h"

static constexpr int kTimeoutMs = 10 * 1000;

QnTestCameraStreamReader::QnTestCameraStreamReader(
    const QnTestCameraResourcePtr& res)
    :
    CLServerPushStreamReader(res)
{
    NX_VERBOSE(this, "%1()", __func__);
    m_socket = nx::network::SocketFactory::createStreamSocket();
    m_socket->setRecvTimeout(kTimeoutMs);
}

QnTestCameraStreamReader::~QnTestCameraStreamReader()
{
    NX_VERBOSE(this, "%1()", __func__);
    stop();
}

bool QnTestCameraStreamReader::receiveData(
    void* buffer, int size, const QString& dataCaptionForErrorMessage)
{
    int totalBytesRead = 0;
    while (totalBytesRead < size)
    {
        const int bytesRead = m_socket->recv(
            (uint8_t*) buffer + totalBytesRead, size - totalBytesRead);
        if (bytesRead <= 0)
        {
            const auto lastOsErrorText = SystemError::getLastOSErrorText();
            const auto lastOsErrorCode = SystemError::getLastOSErrorCode();
            const QString errorMessage = (bytesRead == 0)
                ? "Connection closed."
                : lm("%1 (OS code %2).").args(lastOsErrorText, lastOsErrorCode);
            NX_ERROR(this, "Unable to receive %1 (have read %2 of %3 bytes): %4",
                dataCaptionForErrorMessage, totalBytesRead, size, errorMessage);
            return false;
        }
        totalBytesRead += bytesRead;
    }

    return NX_ASSERT(totalBytesRead == size);
}

QnAbstractMediaDataPtr QnTestCameraStreamReader::getNextData()
{
    NX_VERBOSE(this, "%1() BEGIN", __func__);

    if (!isStreamOpened())
    {
        NX_VERBOSE(this, "%1() END -> null: stream is not opened", __func__);
        return nullptr;
    }

    if (needMetadata())
    {
        const auto result = getMetadata();
        NX_VERBOSE(this, "%1() END -> Metadata", __func__);
        return result;
    }

    const auto result = receivePacket();
    if (!result)
    {
        closeStream();
        NX_VERBOSE(this, "%1() END -> null: receivePacket() failed", __func__);
        return nullptr;
    }

    NX_VERBOSE(this, "%1() END -> MediaData", __func__);
    return result;
}

QnAbstractMediaDataPtr QnTestCameraStreamReader::receivePacket()
{
    const auto error = //< Intended to be called as: return error("message template", ...);
        [this](auto... args)
        {
            NX_ERROR(this, args...);
            return nullptr;
        };

    using namespace nx::vms::testcamera::packet;

    Header header;
    if (!receiveData(&header, sizeof(header), "packet header"))
        return nullptr;

    if (header.codecId() == 0)
        return error("Invalid codec id received: 0.");

    if (header.dataSize() <= 0 || header.dataSize() > 8 * 1024 * 1024)
        return error("Invalid data size received: %1; expected up to 8 MB.", header.dataSize());

    if (header.flags() & Flag::mediaContext)
    {
        // Receive and process the media context packet, and then call this function recursively to
        // receive the next packet (hopefully a frame packet).
        //
        // ATTENTION: If a large number of media context packets is sent, stack overflow may occur.

        if (header.flags() & Flag::ptsIncluded || header.flags() & Flag::channelNumberIncluded)
            return error("Invalid packet flags received: %1", header.flagsAsHex());

        QByteArray mediaContext(header.dataSize(), /*filler*/ 0);
        if (!receiveData(mediaContext.data(), mediaContext.size(), "media context"))
            return nullptr;

        m_context = QnConstMediaContextPtr(QnBasicMediaContext::deserialize(mediaContext));
        return getNextData(); //< recursion
    }

    return receiveFramePacketBody(header);
}

QnAbstractMediaDataPtr QnTestCameraStreamReader::receiveFramePacketBody(
    const nx::vms::testcamera::packet::Header& header)
{
    using namespace nx::vms::testcamera::packet;

    const auto frame = std::make_shared<QnWritableCompressedVideoData>(
        CL_MEDIA_ALIGNMENT, header.dataSize(), m_context);

    if (header.flags() & Flag::ptsIncluded)
    {
        if (!receiveData<PtsUs>(&frame->timestamp, "pts"))
            return nullptr;
    }
    else
    {
        frame->timestamp = qnSyncTime->currentMSecsSinceEpoch() * 1000;
    }

    if (header.flags() & Flag::channelNumberIncluded)
    {
        if (!receiveData<uint8_t>(&frame->channelNumber, "channel number"))
            return nullptr;
        NX_VERBOSE(this, "Received channel number %1.", frame->channelNumber);
    }

    frame->compressionType = header.codecId();
    frame->m_data.finishWriting(header.dataSize());

    if (header.flags() & Flag::keyFrame)
        frame->flags |= QnAbstractMediaData::MediaFlags_AVKey;

    if (!receiveData(frame->m_data.data(), header.dataSize(), "frame data"))
        return nullptr;

    return frame;
}

CameraDiagnostics::Result QnTestCameraStreamReader::openStreamInternal(
    bool /*isCameraControlRequired*/, const QnLiveStreamParams& params)
{
    NX_VERBOSE(this, "%1() BEGIN", __func__);

    using namespace CameraDiagnostics;

    const auto success = //< Intended to be called as: return success();
        [this, func = __func__]()
        {
            const auto result = NoErrorResult();
            NX_VERBOSE(this, "%1() END -> %2", func, result.toString(
                serverModule()->resourcePool()));
            return result;
        };

    const auto error = //< Intended to be called as: return error(...);
        [this, func = __func__](auto result)
        {
            NX_VERBOSE(this, "%1() END -> %2", func, result.toString(
                serverModule()->resourcePool()));
            return result;
        };

    if (isStreamOpened())
        return success();

    nx::utils::Url url = nx::utils::url::parseUrlFields(m_resource->getUrl());
    if (!url.query().isEmpty())
    {
        closeStream();
        return error(CameraInvalidParams(lm("Non-empty query: %1").args(url)));
    }
    url.setQuery(lm("primary=%1&fps=%2").args(
        getRole() == Qn::CR_LiveVideo ? "1" : "0", params.fps));

    if (m_socket->isClosed())
    {
        QnMutexLocker lock(&m_socketMutex);
        if (needToStop())
            return success();

        m_socket = nx::network::SocketFactory::createStreamSocket();
    }

    m_socket->setRecvTimeout(kTimeoutMs);
    m_socket->setSendTimeout(kTimeoutMs);

    NX_VERBOSE(this, "Sending data to URL [%1]", url);
    if (!m_socket->connect(
        url.host(), url.port(), nx::network::deprecated::kDefaultConnectTimeout))
    {
        closeStream();
        return error(CannotOpenCameraMediaPortResult(url, url.port()));
    }

    const QByteArray data = url.toString(QUrl::RemoveAuthority | QUrl::RemoveScheme).toUtf8();
    if (m_socket->send(data.data(), data.size() + 1) <= 0)
    {
       closeStream();
       return error(ConnectionClosedUnexpectedlyResult(url.host(), url.port()));
    }

    return success();
}

void QnTestCameraStreamReader::closeStream()
{
    NX_VERBOSE(this, "%1()", __func__);
    QnMutexLocker lock(&m_socketMutex);
    m_socket->close();
}

bool QnTestCameraStreamReader::isStreamOpened() const
{
    QnMutexLocker lock(&m_socketMutex);
    const bool result = m_socket->isConnected();
    NX_VERBOSE(this, "%1() -> %2", __func__, result ? "true" : "false");
    return result;
}

void QnTestCameraStreamReader::pleaseStop()
{
    NX_VERBOSE(this, "%1() BEGIN", __func__);
    CLServerPushStreamReader::pleaseStop();
    QnMutexLocker lock(&m_socketMutex);
    m_socket->shutdown();
    NX_VERBOSE(this, "%1() END", __func__);
}

#endif // defined(ENABLE_TEST_CAMERA)
