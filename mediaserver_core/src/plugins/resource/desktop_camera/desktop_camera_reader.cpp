
#ifdef ENABLE_DESKTOP_CAMERA

#include <QTextStream>
#include "desktop_camera_reader.h"
#include "utils/common/sleep.h"
#include <nx/utils/log/log.h>
#include "network/tcp_connection_priv.h"
#include "desktop_camera_resource_searcher.h"
#include "desktop_camera_resource.h"
#include <nx/network/http/custom_headers.h>

static const int KEEP_ALIVE_INTERVAL = 30 * 1000;

QnDesktopCameraStreamReader::QnDesktopCameraStreamReader(
    const QnDesktopCameraResourcePtr& res)
:
    CLServerPushStreamReader(res)
{
}

QnDesktopCameraStreamReader::~QnDesktopCameraStreamReader()
{
    stop();
}

CameraDiagnostics::Result QnDesktopCameraStreamReader::openStreamInternal(
    bool isCameraControlRequired, const QnLiveStreamParams& /*params*/)
{
    closeStream();

    if (!m_socket) {
        if (!QnDesktopCameraResourceSearcher::instance())
            return CameraDiagnostics::CannotEstablishConnectionResult(0);

        QString userId = m_resource->getUniqueId();
        m_socket = QnDesktopCameraResourceSearcher::instance()->acquireConnection(userId);
        if (!m_socket)
            return CameraDiagnostics::CannotEstablishConnectionResult(0);
        quint32 cseq = QnDesktopCameraResourceSearcher::instance()->incCSeq(m_socket);
        QString extraHeaders;
        if (!isCameraControlRequired)
            extraHeaders = QString(lit("%1: \r\n")).arg(QLatin1String(Qn::DESKTOP_CAMERA_NO_VIDEO_HEADER_NAME));
        QString request = QString(lit("PLAY %1 RTSP/1.0\r\ncSeq: %2\r\n%3\r\n")).arg("*").arg(cseq).arg(extraHeaders);
        m_socket->send(request.toLatin1());
        m_keepaliveTimer.restart();
    }

    return CameraDiagnostics::Result();
}

void QnDesktopCameraStreamReader::closeStream()
{
    if (m_socket && QnDesktopCameraResourceSearcher::instance())
    {
        quint32 cseq = QnDesktopCameraResourceSearcher::instance()->incCSeq(m_socket);
        QString request = QString(lit("TEARDOWN %1 RTSP/1.0\r\nSeq: %2\r\n\r\n")).arg("*").arg(cseq);
        m_socket->send(request.toLatin1());
        QnDesktopCameraResourceSearcher::instance()->releaseConnection(m_socket);
    }
    m_socket.clear();
}

bool QnDesktopCameraStreamReader::isStreamOpened() const
{
    return m_socket && m_socket->isConnected();
}

void QnDesktopCameraStreamReader::pleaseStop()
{
    QnLongRunnable::pleaseStop();
}

int QnDesktopCameraStreamReader::processTextResponse()
{
    int bufferSize = 4;
    int startPos = QByteArray::fromRawData((char*) m_recvBuffer, bufferSize).indexOf('$');
    while (startPos == -1 && m_socket->isConnected())
    {
        int readed = m_socket->recv(m_recvBuffer + bufferSize, 4);
        if (readed > 0) {
            startPos = QByteArray::fromRawData((char*)m_recvBuffer + bufferSize, 4).indexOf('$');
            if (startPos != -1)
                startPos += bufferSize;
            bufferSize += 4;
        }
    }

    if (startPos == -1)
        return -1;

    QByteArray textMessage = QByteArray::fromRawData((char*)m_recvBuffer, startPos);
    memmove(m_recvBuffer, m_recvBuffer + startPos, bufferSize - startPos);
    return bufferSize - startPos;
}

QnAbstractMediaDataPtr QnDesktopCameraStreamReader::createEmptyPacket()
{
    QnAbstractMediaDataPtr eofData(new QnEmptyMediaData());
    eofData->dataProvider = this;
    return eofData;
}

QnAbstractMediaDataPtr QnDesktopCameraStreamReader::getNextData()
{
    if (!isStreamOpened())
        return QnAbstractMediaDataPtr(0);

    QnAbstractMediaDataPtr result;

    int bufferSize = 0;
    while (!m_needStop && m_socket->isConnected() && !result)
    {
        while (m_socket->isConnected() && bufferSize < 4)
        {
            int readed = m_socket->recv(m_recvBuffer + bufferSize, 4 - bufferSize);
            if (readed > 0)
                bufferSize += readed;
            else {
                m_socket->close();
                return createEmptyPacket();
            }

            if (bufferSize == 4 && (m_recvBuffer[0] != '$' || m_recvBuffer[1] > 1)) // check for streamID [0..1] as well
                bufferSize = processTextResponse();
        }
        if (!m_socket->isConnected())
            return createEmptyPacket();

        int packetSize = (m_recvBuffer[2]<<8) + m_recvBuffer[3];
        quint8 streamIndex = m_recvBuffer[1];
        //NX_ASSERT(streamIndex <= 1);
        bufferSize = 0;
        while (m_socket->isConnected() && bufferSize < packetSize)
        {
            int readed = m_socket->recv(m_recvBuffer + bufferSize, packetSize - bufferSize);
            if (readed > 0) {
                bufferSize += readed;
                if (m_recvBuffer[0] != 0x80) {
                    continue; // not a valid RTP packet. sync lost. find '$' again
                }
            }
            else {
                m_socket->close();
                return createEmptyPacket();
            }
        }
        if (streamIndex >= MEDIA_STREAM_COUNT)
        {
            NX_LOG(lit("QnDesktopCameraStreamReader: wrong streamIndex from pocket (%1 >= %2), pocket rejected")
                   .arg(streamIndex).arg(MEDIA_STREAM_COUNT), cl_logERROR);
        }
        else
        if (bufferSize == packetSize)
        {
            bool gotData;
            m_parsers[streamIndex].processData(m_recvBuffer, 0, packetSize, QnRtspStatistic(), gotData);
            result = m_parsers[streamIndex].nextData();
            if (result)
                result->channelNumber = streamIndex;
        }
        bufferSize = 0;

        if (!m_needStop
            && m_socket->isConnected()
            && m_keepaliveTimer.elapsed() >= KEEP_ALIVE_INTERVAL
            && QnDesktopCameraResourceSearcher::instance())
        {
            quint32 cseq = QnDesktopCameraResourceSearcher::instance()->incCSeq(m_socket);
            QString request = QString(lit("KEEP-ALIVE %1 RTSP/1.0\r\ncSeq: %2\r\n\r\n")).arg("*").arg(cseq);
            if (m_socket->send(request.toLatin1()) < request.size())
            {
                m_socket->close();
                return createEmptyPacket();
            }
            m_keepaliveTimer.restart();
        }
    }

    if (result) {
        result->flags |= QnAbstractMediaData::MediaFlags_LIVE;
        result->opaque = 0;
        if (result->dataType == QnAbstractMediaData::AUDIO && result->context && !m_audioLayout)
        {
            QnMutexLocker lock( &m_audioLayoutMutex );
            m_audioLayout.reset( new QnResourceCustomAudioLayout() );
            QnResourceAudioLayout::AudioTrack track(result->context, "");
            m_audioLayout->addAudioTrack(track);
        }
    }

    return result;
}

void QnDesktopCameraStreamReader::pleaseReopenStream()
{
}

QnConstResourceAudioLayoutPtr QnDesktopCameraStreamReader::getDPAudioLayout() const
{
    QnMutexLocker lock( &m_audioLayoutMutex );
    return m_audioLayout;
}

bool QnDesktopCameraStreamReader::isCameraControlRequired() const
{
    return needConfigureProvider();
}

#endif  //ENABLE_DESKTOP_CAMERA
