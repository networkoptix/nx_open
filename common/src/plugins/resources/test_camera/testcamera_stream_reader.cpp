#include "testcamera_stream_reader.h"
#include "testcamera_resource.h"
#include "utils/common/synctime.h"

static const int TESTCAM_TIMEOUT = 3 * 1000;

QnTestCameraStreamReader::QnTestCameraStreamReader(QnResourcePtr res):
    CLServerPushStreamreader(res)
{
    m_tcpSock.setReadTimeOut(TESTCAM_TIMEOUT);
}

QnTestCameraStreamReader::~QnTestCameraStreamReader()
{
    closeStream();
}

int QnTestCameraStreamReader::receiveData(quint8* buffer, int size)
{
    int done = 0;
    while (done < size)
    {
        int readed = m_tcpSock.recv(buffer + done, size - done);
        if (readed < 1)
            return readed;
        done += readed;
    }
}

QnAbstractMediaDataPtr QnTestCameraStreamReader::getNextData()
{
    if (!isStreamOpened()) {
        openStream();
        if (!isStreamOpened())
            return QnAbstractMediaDataPtr(0);
    }

    quint8 header[5];

    int readed = receiveData(header, sizeof(header));
    if (readed != 5) {
        closeStream();
        return QnAbstractMediaDataPtr();
    }

    int codec = header[0] & 0x7f;
    quint32 size = (header[1] << 24) + (header[2] << 16) + (header[3] << 8) + header[4];
    bool isKeyData = header[0] & 0x80;

    if (size <= 0 || size > 1024*1024*8)
    {
        qWarning() << "Invalid media packet size received. got " << size << "expected < 8Mb";
        closeStream();
        return QnAbstractMediaDataPtr();
    }

    QnAbstractMediaDataPtr rez(new QnCompressedVideoData(CL_MEDIA_ALIGNMENT, size));
    switch(codec)
    {
        case 0:
            rez->compressionType = CODEC_ID_H264;
            break;
        case 1:
            rez->compressionType = CODEC_ID_MJPEG;
            break;
        default:
            qWarning() << "Unknown codec type" << codec << "for test camera" << m_resource->getUrl();
            return QnAbstractMediaDataPtr();
    }
    rez->data.done(size);
    rez->timestamp = qnSyncTime->currentMSecsSinceEpoch();
    if (isKeyData)
        rez->flags |= AV_PKT_FLAG_KEY;

    readed = receiveData((quint8*) rez->data.data(), size);
    if (readed != size) {
        closeStream();
        return QnAbstractMediaDataPtr();
    }

    return rez;
}

void QnTestCameraStreamReader::openStream()
{
    if (isStreamOpened())
        return;
    
    QString ggg = m_resource->getUrl();

    QUrl url(m_resource->getUrl());

    m_tcpSock.setReadTimeOut(TESTCAM_TIMEOUT);
    m_tcpSock.setWriteTimeOut(TESTCAM_TIMEOUT);

    QnNetworkResourcePtr res = qSharedPointerDynamicCast<QnNetworkResource>(m_resource);

    if (m_tcpSock.isClosed())
        m_tcpSock.reopen();
    if (!m_tcpSock.connect(url.host().toLatin1().data(), url.port()))
    {
        closeStream();
        return;
    }
    QByteArray path = url.path().toLocal8Bit();
    m_tcpSock.send(path.data(), path.size());
}

void QnTestCameraStreamReader::closeStream()
{
    m_tcpSock.close();
}

bool QnTestCameraStreamReader::isStreamOpened() const
{
    return m_tcpSock.isConnected();
}

void QnTestCameraStreamReader::updateStreamParamsBasedOnQuality()
{

}

void QnTestCameraStreamReader::updateStreamParamsBasedOnFps()
{

}
