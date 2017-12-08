#ifdef ENABLE_TEST_CAMERA

#include "testcamera_stream_reader.h"

#include <nx/streaming/video_data_packet.h>
#include <nx/streaming/basic_media_context.h>
#include "testcamera_resource.h"
#include "utils/common/synctime.h"

static const int TESTCAM_TIMEOUT = 5 * 1000;

QnTestCameraStreamReader::QnTestCameraStreamReader(const QnResourcePtr& res):
    CLServerPushStreamReader(res)
{
    m_tcpSock = SocketFactory::createStreamSocket();
    m_tcpSock->setRecvTimeout(TESTCAM_TIMEOUT);
}

QnTestCameraStreamReader::~QnTestCameraStreamReader()
{
    stop();
}

int QnTestCameraStreamReader::receiveData(quint8* buffer, int size)
{
    int done = 0;
    while (done < size)
    {
        int readed = m_tcpSock->recv(buffer + done, size - done);
        if (readed < 1)
            return readed;
        done += readed;
    }
    return done;
}

QnAbstractMediaDataPtr QnTestCameraStreamReader::getNextData()
{
    if (!isStreamOpened())
        return QnAbstractMediaDataPtr(0);

    if (needMetadata())
        return getMetadata();

    quint8 header[6];

    quint32 readed = receiveData(header, sizeof(header));
    if (readed != sizeof(header)) {
        closeStream();
        return QnAbstractMediaDataPtr();
    }

    bool isKeyData = header[0] & 0x80;
    bool isCodecContext = header[0] & 0x40;
    quint16 codec = ((header[0]&0x3f) << 8) + header[1];
    quint32 size = (header[2] << 24) + (header[3] << 16) + (header[4] << 8) + header[5];

    if (size <= 0 || size > 1024*1024*8)
    {
        qWarning() << "Invalid media packet size received. got " << size << "expected < 8Mb";
        closeStream();
        return QnAbstractMediaDataPtr();
    }

    if (isCodecContext)
    {
        quint8* ctxData = new quint8[size];
        quint32 readed = receiveData(ctxData, size);

        if (readed == size)
        {
            QByteArray payloadArray((const char *) ctxData, size);
            m_context = QnConstMediaContextPtr(QnBasicMediaContext::deserialize(payloadArray));
        }
        delete [] ctxData;
        return getNextData();
    }

    QnWritableCompressedVideoDataPtr rez(new QnWritableCompressedVideoData(CL_MEDIA_ALIGNMENT, size, m_context));
    rez->compressionType = (AVCodecID) codec;

    rez->m_data.finishWriting(size);
    rez->timestamp = qnSyncTime->currentMSecsSinceEpoch()*1000;
    if (isKeyData)
        rez->flags |= QnAbstractMediaData::MediaFlags_AVKey;

    readed = receiveData((quint8*) rez->m_data.data(), size);
    if (readed != size) {
        closeStream();
        return QnAbstractMediaDataPtr();
    }

    /*
    static QFile* gggFile = 0;
    if (gggFile == 0)
    {
        gggFile = new QFile("c:/dest.264");
        gggFile->open(QFile::WriteOnly);
    }
    gggFile->write(rez->data.data(), rez->data.size());
    gggFile->flush();
    */

    return rez;
}

CameraDiagnostics::Result QnTestCameraStreamReader::openStreamInternal(bool isCameraControlRequired, const QnLiveStreamParams& params)
{
    Q_UNUSED(isCameraControlRequired);
    if (isStreamOpened())
        return CameraDiagnostics::NoErrorResult();

    QString urlStr = m_resource->getUrl();
    QnNetworkResourcePtr res = qSharedPointerDynamicCast<QnNetworkResource>(m_resource);

    urlStr += QString(QLatin1String("?primary=%1&fps=%2")).arg(getRole() == Qn::CR_LiveVideo).arg(params.fps);
    QUrl url(urlStr);


    if (m_tcpSock->isClosed())
    {
        m_tcpSock = SocketFactory::createStreamSocket();
        m_tcpSock->setRecvTimeout(TESTCAM_TIMEOUT);
    }

    m_tcpSock->setRecvTimeout(TESTCAM_TIMEOUT);
    m_tcpSock->setSendTimeout(TESTCAM_TIMEOUT);

    if (!m_tcpSock->connect(url.host(), url.port()))
    {
        closeStream();
        return CameraDiagnostics::CannotOpenCameraMediaPortResult(url.toString(), url.port());
    }
    QByteArray path = urlStr.mid(urlStr.lastIndexOf(QLatin1String("/"))).toUtf8();
    m_tcpSock->send(path.data(), path.size() + 1);

    return CameraDiagnostics::NoErrorResult();
}

void QnTestCameraStreamReader::closeStream()
{
    m_tcpSock->close();
}

bool QnTestCameraStreamReader::isStreamOpened() const
{
    return m_tcpSock->isConnected();
}

#endif // #ifdef ENABLE_TEST_CAMERA
