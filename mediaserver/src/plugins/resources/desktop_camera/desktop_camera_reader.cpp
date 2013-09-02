#include <QTextStream>
#include "desktop_camera_reader.h"
#include "utils/common/sleep.h"
#include "utils/network/tcp_connection_priv.h"
#include "desktop_camera_resource_searcher.h"
#include "desktop_camera_resource.h"

static const int KEEP_ALIVE_TIMEOUT = 1000 * 40;

QnDesktopCameraStreamReader::QnDesktopCameraStreamReader(QnResourcePtr res):
    CLServerPushStreamReader(res),
    m_recvBufferSize(0)
{
}

QnDesktopCameraStreamReader::~QnDesktopCameraStreamReader()
{
    stop();
}

CameraDiagnostics::Result QnDesktopCameraStreamReader::openStream()
{
    QString userName = m_resource.dynamicCast<QnDesktopCameraResource>()->getUserName();
    m_socket = QnDesktopCameraResourceSearcher::instance().getConnection(userName);
    if (!m_socket)
        return CameraDiagnostics::CannotEstablishConnectionResult(0);

    QString request = QString(lit("CONNECT %1 HTTP/1.0\r\n\r\n")).arg("*");
    m_socket->send(request.toLocal8Bit());

    m_keepaliveTimer.restart();

    const CameraDiagnostics::Result result;
    return result;
}

void QnDesktopCameraStreamReader::closeStream()
{
    if (m_socket) {
        QString request = QString(lit("DISCONNECT %1 HTTP/1.0\r\n\r\n")).arg("*");
        m_socket->send(request.toLocal8Bit());
    }
    m_socket.clear();
    m_recvBufferSize = 0;
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

    while (!m_needStop && m_socket->isConnected()) {
        int readed = m_socket->recv(m_recvBuffer + m_recvBufferSize, sizeof(m_recvBuffer) - m_recvBufferSize);
        if (readed > 0)
            m_recvBufferSize += readed;
        if (m_recvBufferSize < 4)
            continue;
        
        int packetSize = (m_recvBuffer[2]<<8) + m_recvBuffer[3] + 2;
        while (!m_needStop && m_socket->isConnected() && m_recvBufferSize < packetSize)
        {
            readed = m_socket->recv(m_recvBuffer + m_recvBufferSize, sizeof(m_recvBuffer) - m_recvBufferSize);
            if (readed > 0)
                m_recvBufferSize += readed;
        }
        if (m_recvBufferSize >= packetSize)
        {
            parser.processData(m_recvBuffer + 4, 0, packetSize - 4, RtspStatistic(), result);
        }
        memmove(m_recvBuffer, m_recvBuffer + packetSize, m_recvBufferSize - packetSize);
        m_recvBufferSize -= packetSize;
    }

    if (!m_needStop && m_socket->isConnected() && m_keepaliveTimer.elapsed() >= KEEP_ALIVE_TIMEOUT) {
        QString request = QString(lit("KEEP-ALIVE %1 HTTP/1.0\r\n\r\n")).arg("*");
        m_socket->send(request.toLocal8Bit());
        m_keepaliveTimer.restart();
    }

    return result;
}

void QnDesktopCameraStreamReader::updateStreamParamsBasedOnQuality()
{
}

void QnDesktopCameraStreamReader::updateStreamParamsBasedOnFps()
{
}
