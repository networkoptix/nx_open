#include "testcamera_stream_reader.h"
#if defined(ENABLE_TEST_CAMERA)

#include <nx/utils/log/log.h>

#include <nx/streaming/video_data_packet.h>
#include <nx/streaming/basic_media_context.h>
#include <nx/vms/testcamera/packet.h>
#include "testcamera_resource.h"
#include "utils/common/synctime.h"

static constexpr int kTimeoutMs = 5 * 1000;

QnTestCameraStreamReader::QnTestCameraStreamReader(
    const QnTestCameraResourcePtr& res)
    :
    CLServerPushStreamReader(res)
{
    m_socket = nx::network::SocketFactory::createStreamSocket();
    m_socket->setRecvTimeout(kTimeoutMs);
}

QnTestCameraStreamReader::~QnTestCameraStreamReader()
{
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
        if (bytesRead < 1)
        {
            NX_VERBOSE(this, "STREAM ERROR: Unable to receive %1 (have read %2 of %3 bytes): %4",
                dataCaptionForErrorMessage, totalBytesRead, size,
                (bytesRead == 0) ? "Connection closed." : SystemError::getLastOSErrorText());
            return false;
        }
        totalBytesRead += bytesRead;
    }

    return NX_ASSERT(totalBytesRead == size);
}

QnAbstractMediaDataPtr QnTestCameraStreamReader::getNextData()
{
    if (!isStreamOpened())
        return nullptr;

    if (needMetadata())
        return getMetadata();

    const auto error = //< Intended to be called as `return error("message template", ...);`.
        [this](auto... args)
        {
            NX_ERROR(this, args...);
            closeStream();
            return nullptr;
        };

    using namespace nx::vms::testcamera::packet;

    Header header;
    if (!receiveData(&header, sizeof(header), "packet header"))
        return nullptr;

    if (header.codecId() == 0)
        return error("STREAM ERROR: Codec id is 0.");

    if (header.dataSize() <= 0 || header.dataSize() > 8 * 1024 * 1024)
        return error("Invalid data size received: %1; expected up to 8 MB.", header.dataSize());

    if (header.flags() & Flag::mediaContext)
    {
        if (header.flags() & Flag::ptsIncluded)
            return error("Invalid packet flags received: %1", header.flagsAsHex());

        QByteArray mediaContext(header.dataSize(), /*filler*/ 0);
        if (!receiveData(mediaContext.data(), mediaContext.size(), "media context"))
            return nullptr;

        m_context = QnConstMediaContextPtr(QnBasicMediaContext::deserialize(mediaContext));
        return getNextData(); //< recursion
    }
    else
    {
        return receiveFrame(header);
    }
}

QnAbstractMediaDataPtr QnTestCameraStreamReader::receiveFrame(
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
    if (isStreamOpened())
        return CameraDiagnostics::NoErrorResult();

    nx::utils::Url url = nx::utils::url::parseUrlFields(m_resource->getUrl());
    if (url.query() != QString())
    {
        closeStream();
        return CameraDiagnostics::CameraInvalidParams(lm("Not empty query: [%1]").args(url));
    }
    url.setQuery(lm("primary=%1&fps=%2").args(
        getRole() == Qn::CR_LiveVideo ? "1" : "0", params.fps));

    if (m_socket->isClosed())
    {
        QnMutexLocker lock(&m_socketMutex);
        if (needToStop())
            return CameraDiagnostics::NoErrorResult();

        m_socket = nx::network::SocketFactory::createStreamSocket();
        m_socket->setRecvTimeout(kTimeoutMs);
    }

    m_socket->setRecvTimeout(kTimeoutMs);
    m_socket->setSendTimeout(kTimeoutMs);

    NX_VERBOSE(this, "Sending data to URL [%1]", url);
    if (!m_socket->connect(
        url.host(), url.port(), nx::network::deprecated::kDefaultConnectTimeout))
    {
        closeStream();
        return CameraDiagnostics::CannotOpenCameraMediaPortResult(url.toString(), url.port());
    }

    const QByteArray data = url.toString(QUrl::RemoveAuthority | QUrl::RemoveScheme).toUtf8();
    if (m_socket->send(data.data(), data.size() + 1) <= 0)
    {
       closeStream();
       return CameraDiagnostics::ConnectionClosedUnexpectedlyResult(url.toString(), url.port());
    }

    return CameraDiagnostics::NoErrorResult();
}

void QnTestCameraStreamReader::closeStream()
{
    QnMutexLocker lock(&m_socketMutex);
    m_socket->close();
}

bool QnTestCameraStreamReader::isStreamOpened() const
{
    QnMutexLocker lock(&m_socketMutex);
    return m_socket->isConnected();
}

void QnTestCameraStreamReader::pleaseStop()
{
    CLServerPushStreamReader::pleaseStop();
    QnMutexLocker lock(&m_socketMutex);
    m_socket->shutdown();
}

#endif // defined(ENABLE_TEST_CAMERA)
