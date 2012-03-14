#include "droid_stream_reader.h"
#include "droid_resource.h"

static const int DROID_TIMEOUT = 3 * 1000;

PlDroidStreamReader::PlDroidStreamReader(QnResourcePtr res):
    CLServerPushStreamreader(res),
    m_rtpSession(),
    m_ioDevice(m_rtpSession),
    m_h264Parser(&m_ioDevice),
    m_videoSock(0),
    m_audioSock(0),
    m_dataSock(0)
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

    QnAbstractMediaDataPtr rez = m_h264Parser.getNextData();
    if (!rez)
        closeStream();
    return rez;

    /*
    char buff[1024*8];

    int readed = m_videoSock.recv(buff, sizeof(buff));
    
    int gg = 4;

    return QnAbstractMediaDataPtr(0);
    */
    /*
    while (1)
    {
        int readed = m_sock->recv(buff, 1460);

        fwrite(buff, readed, 1, f);

        if (readed < 1)
        {
            fclose(f);
            return QnAbstractMediaDataPtr(0);
        }
    }

    fclose(f);
    */

    /*

    quint16 chunkSize = 0;
    int readed = m_sock->recv((char*)&chunkSize, 2);
    if (readed < 2)
        return QnAbstractMediaDataPtr(0);


    QnCompressedVideoDataPtr videoData(new QnCompressedVideoData(CL_MEDIA_ALIGNMENT, chunkSize+FF_INPUT_BUFFER_PADDING_SIZE));
    char* curPtr = videoData->data.data();
    videoData->data.prepareToWrite(chunkSize); // this call does nothing 
    videoData->data.done(chunkSize);

    int dataLeft = chunkSize;

    while (dataLeft > 0)
    {
        int readed = m_sock->recv(curPtr, dataLeft);
        if (readed < 1)
            return QnAbstractMediaDataPtr(0);
        curPtr += readed;
        dataLeft -= readed;
    }

    FILE* f = fopen("c:/droid.mp4", "ab");
    fwrite(videoData->data.data(), chunkSize, 1, f);
    fclose(f);


    videoData->compressionType = CODEC_ID_H264;
    videoData->width = 320;
    videoData->height = 240;

    
    videoData->flags |= AV_PKT_FLAG_KEY;

    videoData->channelNumber = 0;
    videoData->timestamp = qnSyncTime->currentMSecsSinceEpoch() * 1000;

    return videoData;

    /**/

}

void PlDroidStreamReader::openStream()
{
    if (isStreamOpened())
        return;
    
    QString portStr = m_resource->getUrl();
    if (!portStr.startsWith("raw://"))
        return;
    portStr = portStr.mid(QString("raw://").length());

    QStringList ports = portStr.split(',');
    if (ports.size() < 4) {
        qWarning() << "Invalid droid URL format. Expected at least 4 ports";
        return;
    }

    if (ports[0].contains(':'))
        m_connectionPort = ports[0].mid(ports[0].indexOf(':')+1).toInt();
    m_videoPort = ports[1].toInt();
    m_audioPort = ports[2].toInt();
    m_dataPort = ports[3].toInt();

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
    m_audioSock = new UDPSocket();
    m_dataSock = new UDPSocket();

    m_ioDevice.setSocket(m_videoSock);

    m_videoSock->setReadTimeOut(DROID_TIMEOUT);
    m_audioSock->setReadTimeOut(DROID_TIMEOUT);
    m_dataSock->setReadTimeOut(DROID_TIMEOUT);

    if (!m_videoSock->setLocalPort(m_videoPort))
    {
        qWarning() << "Can't open video port for droid device. Port already in use";
        closeStream();
        return;
    }

    /*
    if (!m_audioSock.setLocalPort(m_audioPort))
    {
        qWarning() << "Can't open audio port for droid device. Port already in use";
        closeStream();
        return;
    }
    */

    m_h264Parser.setSDPInfo("a=rtpmap:96 H264/90000\na=fmtp:96 packetization-mode=1;profile-level-id=42001e;sprop-parameter-sets=Z0IAHukBQHsg,aM4BDyA=;");

    if (!m_dataSock->setLocalPort(m_dataPort))
    {
        qWarning() << "Can't open audio port for droid device. Port already in use";
        closeStream();
        return;
    }

    // todo: extract resolution here from control port
    // current version of droid software does not send data to control port
    //quint8 dataBuff[1024];
    //int readed = m_dataSock.recv(dataBuff, sizeof(dataBuff));
}

void PlDroidStreamReader::closeStream()
{
    delete m_videoSock;
    delete m_audioSock;
    delete m_dataSock;

    m_videoSock = 0;
    m_audioSock = 0;
    m_dataSock = 0;

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
