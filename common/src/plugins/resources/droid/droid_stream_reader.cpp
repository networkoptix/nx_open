#include "droid_stream_reader.h"
#include "droid_resource.h"
#include "droid_controlport_listener.h"

static const int DROID_TIMEOUT = 3 * 1000;


QMutex PlDroidStreamReader::m_allReadersMutex;
QMap<quint32, PlDroidStreamReader*> PlDroidStreamReader::m_allReaders;


void PlDroidStreamReader::setSDPInfo(quint32 ipv4, QByteArray sdpInfo)
{
    QMutexLocker lock(&m_allReadersMutex);
    PlDroidStreamReader* reader = m_allReaders.value(ipv4);
    if (reader)
        reader->setSDPInfo(sdpInfo);
}

PlDroidStreamReader::PlDroidStreamReader(QnResourcePtr res):
    CLServerPushStreamreader(res),
    QnLiveStreamProvider(res),
    m_rtpSession(),
    m_videoIoDevice(0),
    m_audioIoDevice(0),
    m_h264Parser(0),
    m_gotSDP(0)
{
    m_droidRes = qSharedPointerDynamicCast<QnDroidResource>(res);

    m_connectionPort = 0;
    m_videoPort = 0;
    m_audioPort = 0;
    m_dataPort = 0;
}

PlDroidStreamReader::~PlDroidStreamReader()
{
    closeStream();
}

QnAbstractMediaDataPtr PlDroidStreamReader::getNextData()
{
    QnAbstractMediaDataPtr result;
    QnByteArray rtpBuffer(16, 1024*64);

    if (!isStreamOpened())
        return QnAbstractMediaDataPtr(0);

    while (!m_needStop && !result)
    {
        if (!m_gotSDP) {
            msleep(10);
            continue;
        }

        QMutexLocker lock(&m_controlPortSync);
        rtpBuffer.reserve(rtpBuffer.size() + MAX_RTP_PACKET_SIZE);
        int readed = m_videoIoDevice->read( (char*) rtpBuffer.data() + rtpBuffer.size(), MAX_RTP_PACKET_SIZE);
        if (readed > 0) {
            m_h264Parser->processData((quint8*)rtpBuffer.data(), rtpBuffer.size(), readed, m_videoIoDevice->getStatistic(), result );
            rtpBuffer.finishWriting(readed);
        }
    }
    return result;
}

void PlDroidStreamReader::openStream()
{
    m_gotSDP =  false;
    if (isStreamOpened())
        return;
    
    QString portStr = m_resource->getUrl();
    if (!portStr.startsWith(QLatin1String("raw://")))
        return;
    portStr = portStr.mid(QString(QLatin1String("raw://")).length());

    QStringList ports = portStr.split(QLatin1Char(','));
    if (ports.size() < 2) {
        qWarning() << "Invalid droid URL format. Expected at least 4 ports";
        return;
    }

    if (ports[0].contains(QLatin1Char(':')))
        m_connectionPort = ports[0].mid(ports[0].indexOf(QLatin1Char(':'))+1).toInt();
    
    //m_videoPort = ports[1].toInt();
    //m_audioPort = ports[2].toInt();
    m_dataPort = ports[1].toInt();

    m_tcpSock.setReadTimeOut(DROID_TIMEOUT);
    m_tcpSock.setWriteTimeOut(DROID_TIMEOUT);

    QnNetworkResourcePtr res = qSharedPointerDynamicCast<QnNetworkResource>(m_resource);
    QString host = res->getHostAddress().toString();

    if (m_tcpSock.isClosed())
        m_tcpSock.reopen();
    if (!m_tcpSock.connect(host, m_connectionPort))
    {
        closeStream();
        return;
    }

    m_videoIoDevice = new RTPIODevice(&m_rtpSession, false);
    m_audioIoDevice = new RTPIODevice(&m_rtpSession, false);
    m_h264Parser = new CLH264RtpParser();

    {
        QMutexLocker lock(&m_allReadersMutex);
        quint32 ip = res->getHostAddress().toIPv4Address();
        m_allReaders.insert(ip, this);
    }
    QByteArray request = QString(QLatin1String("v:%1,a:%2,f:%3")).arg(m_videoIoDevice->getMediaSocket()->getLocalPort()).
                            arg(m_audioIoDevice->getMediaSocket()->getLocalPort()).arg(DROID_CONTROL_TCP_SERVER_PORT).toAscii();
    
    int sendLen = m_tcpSock.send(request.data(), request.size());
    if (sendLen != request.size())
    {
        qWarning() << "Can't send request to droid device.";
        closeStream();
        return;
    }

    /*
    m_incomeTCPData = m_dataSock->accept();
    if (m_incomeTCPData)
    {
        quint8 recvBuffer[1024];
        int readed = m_incomeTCPData->recv(recvBuffer, sizeof(recvBuffer));
        if (readed > 0)
        {
            m_h264Parser.setSDPInfo(QByteArray((const char*)recvBuffer, readed));
        }
    }
    */
}

void PlDroidStreamReader::closeStream()
{
    {
        QMutexLocker lock(&m_allReadersMutex);
        quint32 ip = m_droidRes->getHostAddress().toIPv4Address();
        m_allReaders.remove(ip);
    }

    delete m_videoIoDevice;
    delete m_audioIoDevice;
    delete m_h264Parser;
    m_videoIoDevice = 0;
    m_audioIoDevice = 0;
    m_h264Parser = 0;

    m_tcpSock.close();
}

bool PlDroidStreamReader::isStreamOpened() const
{
    return m_tcpSock.isConnected() && m_videoIoDevice != 0;
}

void PlDroidStreamReader::updateStreamParamsBasedOnQuality()
{

}

void PlDroidStreamReader::updateStreamParamsBasedOnFps()
{

}

void PlDroidStreamReader::setSDPInfo(QByteArray sdpInfo)
{
    QMutexLocker lock(&m_controlPortSync);
    m_h264Parser->setSDPInfo(sdpInfo.split('\n'));
    m_gotSDP = true;
}
