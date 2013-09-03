#include <QTextStream>
#include "desktop_camera_reader.h"
#include "utils/common/sleep.h"
#include "utils/network/tcp_connection_priv.h"
#include "desktop_camera_resource_searcher.h"
#include "desktop_camera_resource.h"

static const int KEEP_ALIVE_TIMEOUT = 1000 * 40;

QnDesktopCameraStreamReader::QnDesktopCameraStreamReader(QnResourcePtr res):
    CLServerPushStreamReader(res)
{
}

QnDesktopCameraStreamReader::~QnDesktopCameraStreamReader()
{
    stop();
}

CameraDiagnostics::Result QnDesktopCameraStreamReader::openStream()
{
   if (!m_socket) {
        QString userName = m_resource.dynamicCast<QnDesktopCameraResource>()->getUserName();
        m_socket = QnDesktopCameraResourceSearcher::instance().getConnection(userName);
        if (!m_socket)
            return CameraDiagnostics::CannotEstablishConnectionResult(0);
        QString request = QString(lit("CONNECT %1 HTTP/1.0\r\n\r\n")).arg("*");
        m_socket->send(request.toLocal8Bit());
        m_keepaliveTimer.restart();
    }

    return CameraDiagnostics::Result();
}

void QnDesktopCameraStreamReader::closeStream()
{
    if (m_socket) {
        QString request = QString(lit("DISCONNECT %1 HTTP/1.0\r\n\r\n")).arg("*");
        m_socket->send(request.toLocal8Bit());
        QString userName = m_resource.dynamicCast<QnDesktopCameraResource>()->getUserName();
        QnDesktopCameraResourceSearcher::instance().releaseConnection(userName);
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

QnAbstractMediaDataPtr QnDesktopCameraStreamReader::getNextData()
{
    if (!isStreamOpened()) {
        openStream();
        if (!isStreamOpened())
            return QnAbstractMediaDataPtr(0);
    }

    QnAbstractMediaDataPtr result;

    int bufferSize = 0;
    while (!m_needStop && m_socket->isConnected() && !result) 
    {
        if (bufferSize < 4) {
            int readed = m_socket->recv(m_recvBuffer + bufferSize, 4 - bufferSize);
            if (readed > 0)
                bufferSize += readed;
            continue;
        }
        
        int packetSize = (m_recvBuffer[2]<<8) + m_recvBuffer[3];
        bufferSize = 0;
        while (!m_needStop && m_socket->isConnected() && bufferSize < packetSize)
        {
            int readed = m_socket->recv(m_recvBuffer + bufferSize, packetSize - bufferSize);
            if (readed > 0)
                bufferSize += readed;
        }
        if (bufferSize == packetSize)
        {
            parser.processData(m_recvBuffer, 0, packetSize, RtspStatistic(), result);
        }
        bufferSize = 0;

        if (!m_needStop && m_socket->isConnected() && m_keepaliveTimer.elapsed() >= KEEP_ALIVE_TIMEOUT) {
            QString request = QString(lit("KEEP-ALIVE %1 HTTP/1.0\r\n\r\n")).arg("*");
            m_socket->send(request.toLocal8Bit());
            m_keepaliveTimer.restart();
        }
    }

    if (result) {
        result->flags |= QnAbstractMediaData::MediaFlags_LIVE;
        result->opaque = 0;
    }

    return result;
}

void QnDesktopCameraStreamReader::updateStreamParamsBasedOnQuality()
{
}

void QnDesktopCameraStreamReader::updateStreamParamsBasedOnFps()
{
}
