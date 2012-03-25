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
    m_rtpSession(),
    m_ioDevice(m_rtpSession),
    m_h264Parser(&m_ioDevice),
    m_videoSock(0),
    m_audioSock(0),
    m_gotSDP(0)
{
    m_droidRes = qSharedPointerDynamicCast<QnDroidResource>(res);

    m_connectionPort = 0;
    m_videoPort = 0;
    m_audioPort = 0;
    m_dataPort = 0;

    m_ioDevice.setTCPMode(false);
}

PlDroidStreamReader::~PlDroidStreamReader()
{
    closeStream();
}

QnAbstractMediaDataPtr PlDroidStreamReader::getNextData()
{
    if (!isStreamOpened())
        return QnAbstractMediaDataPtr(0);

    QnAbstractMediaDataPtr rez;
    while (!m_needStop && rez == 0)
    {
        if (!m_gotSDP) {
            msleep(10);
            continue;
        }

        QMutexLocker lock(&m_controlPortSync);
        rez = m_h264Parser.getNextData();
    }
    return rez;
}

void PlDroidStreamReader::openStream()
{
    m_gotSDP =  false;
    if (isStreamOpened())
        return;
    
    QString portStr = m_resource->getUrl();
    if (!portStr.startsWith("raw://"))
        return;
    portStr = portStr.mid(QString("raw://").length());

    QStringList ports = portStr.split(',');
    if (ports.size() < 2) {
        qWarning() << "Invalid droid URL format. Expected at least 4 ports";
        return;
    }

    if (ports[0].contains(':'))
        m_connectionPort = ports[0].mid(ports[0].indexOf(':')+1).toInt();
    
    //m_videoPort = ports[1].toInt();
    //m_audioPort = ports[2].toInt();
    m_dataPort = ports[1].toInt();

    m_tcpSock.setReadTimeOut(DROID_TIMEOUT);
    m_tcpSock.setWriteTimeOut(DROID_TIMEOUT);

    QnNetworkResourcePtr res = qSharedPointerDynamicCast<QnNetworkResource>(m_resource);
    QString host = res->getHostAddress().toString();

    if (m_tcpSock.isClosed())
        m_tcpSock.reopen();
    if (!m_tcpSock.connect(host.toLatin1().data(), m_connectionPort))
    {
        closeStream();
        return;
    }

    m_videoSock = new UDPSocket();
    m_videoSock->setLocalPort(0);
    m_audioSock = new UDPSocket();
    m_audioSock->setLocalPort(0);

    m_videoSock->setReadTimeOut(DROID_TIMEOUT);
    m_audioSock->setReadTimeOut(DROID_TIMEOUT);

    {
        QMutexLocker lock(&m_allReadersMutex);
        quint32 ip = res->getHostAddress().toIPv4Address();
        m_allReaders.insert(ip, this);
    }

    QByteArray request = QString("v:%1,a:%2,f:%3").arg(m_videoSock->getLocalPort()).arg(m_audioSock->getLocalPort()).arg(DROID_CONTROL_TCP_SERVER_PORT).toAscii();
    
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

    m_ioDevice.setSocket(m_videoSock);
}

void PlDroidStreamReader::closeStream()
{
    {
        QMutexLocker lock(&m_allReadersMutex);
        quint32 ip = m_droidRes->getHostAddress().toIPv4Address();
        m_allReaders.remove(ip);
    }

    delete m_videoSock;
    delete m_audioSock;

    m_videoSock = 0;
    m_audioSock = 0;

    m_tcpSock.close();
}

bool PlDroidStreamReader::isStreamOpened() const
{
    return m_tcpSock.isConnected() && m_videoSock != 0;
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
    m_h264Parser.setSDPInfo(sdpInfo);
    m_gotSDP = true;
}
