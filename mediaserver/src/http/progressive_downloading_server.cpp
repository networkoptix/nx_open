#include <QFileInfo>
#include "progressive_downloading_server.h"
#include "utils/network/tcp_connection_priv.h"
#include "utils/network/tcp_listener.h"
#include "transcoding/transcoder.h"
#include "transcoding/ffmpeg_transcoder.h"
#include "camera/video_camera.h"
#include "core/resourcemanagment/resource_pool.h"
#include "camera/camera_pool.h"
#include "core/dataconsumer/dataconsumer.h"

static const int CONNECTION_TIMEOUT = 1000 * 5;

QnProgressiveDownloadingServer::QnProgressiveDownloadingServer(const QHostAddress& address, int port):
    QnTcpListener(address, port)
{

}

QnProgressiveDownloadingServer::~QnProgressiveDownloadingServer()
{

}

QnTCPConnectionProcessor* QnProgressiveDownloadingServer::createRequestProcessor(TCPSocket* clientSocket, QnTcpListener* owner)
{
    return new QnProgressiveDownloadingConsumer(clientSocket, owner);
}

// -------------------------- QnProgressiveDownloadingDataConsumer ---------------------

class QnProgressiveDownloadingDataConsumer: public QnAbstractDataConsumer
{
public:
    QnProgressiveDownloadingDataConsumer (QnProgressiveDownloadingConsumer* owner): m_owner(owner), QnAbstractDataConsumer(50) {}
    void sendResponse()
    {
        m_needStop = false;
        bool isr = isRunning();
        run();
        int gg = 4;
    }
protected:
    virtual bool processData(QnAbstractDataPacketPtr data) override
    {
        QnByteArray result(CL_MEDIA_ALIGNMENT, 0);
        QnAbstractMediaDataPtr media = qSharedPointerDynamicCast<QnAbstractMediaData>(data);
        int errCode = m_owner->getTranscoder()->transcodePacket(media, result);
        if (errCode == 0) {
            if (result.size() > 0) {
                if (!m_owner->sendChunk(result))
                    m_needStop = true;
            }
        }
        else
            m_needStop = true;
        return true;
    }
private:
    QnProgressiveDownloadingConsumer* m_owner;
};

// -------------- QnProgressiveDownloadingConsumer -------------------

class QnProgressiveDownloadingConsumer::QnProgressiveDownloadingConsumerPrivate: public QnTCPConnectionProcessor::QnTCPConnectionProcessorPrivate
{
public:
    QnFfmpegTranscoder transcoder;
    QByteArray streamingFormat;
    CodecID videoCodec;
};

QnProgressiveDownloadingConsumer::QnProgressiveDownloadingConsumer(TCPSocket* socket, QnTcpListener* _owner):
    QnTCPConnectionProcessor(new QnProgressiveDownloadingConsumerPrivate, socket, _owner)
{
    Q_D(QnProgressiveDownloadingConsumer);
    d->socketTimeout = CONNECTION_TIMEOUT;
    d->streamingFormat = "webm";
    d->videoCodec = CODEC_ID_VP8;
}

QnProgressiveDownloadingConsumer::~QnProgressiveDownloadingConsumer()
{
}

QByteArray QnProgressiveDownloadingConsumer::getMimeType(QByteArray streamingFormat)
{
    if (streamingFormat == "webm")
        return "video/webm";
    else if (streamingFormat == "mpegts")
        return "video/mp2t";
    else 
        return QByteArray();
}

void QnProgressiveDownloadingConsumer::run()
{
    Q_D(QnProgressiveDownloadingConsumer);
    if (d->transcoder.setContainer(d->streamingFormat) != 0)
    //if (d->transcoder.setContainer("mpegts") != 0)
    {
        QByteArray msg;
        msg = QByteArray("Transcoding error. Can not setup output format:") + d->transcoder.getLastErrorMessage().toLocal8Bit();
        qWarning() << msg;
        d->responseBody = msg;
        sendResponse("HTTP", CODE_INTERNAL_ERROR, "plain/text");
        return;
    }

    if (d->transcoder.setVideoCodec(d->videoCodec, QnTranscoder::TM_FfmpegTranscode, QSize(640,480)) != 0)
    //if (d->transcoder.setVideoCodec(CODEC_ID_MPEG2VIDEO, QnTranscoder::TM_FfmpegTranscode, QSize(640,480)) != 0)
    {
        QByteArray msg;
        msg = QByteArray("Transcoding error. Can not setup video codec:") + d->transcoder.getLastErrorMessage().toLocal8Bit();
        qWarning() << msg;
        d->responseBody = msg;
        sendResponse("HTTP", CODE_INTERNAL_ERROR, "plain/text");
        return;
    }

    QnAbstractMediaStreamDataProviderPtr dataProvider;

    d->socket->setReadTimeOut(1000*1000);
    d->socket->setWriteTimeOut(1000*1000);

    if (readRequest())
    {
        parseRequest();
        d->responseBody.clear();
        QFileInfo fi(getDecodedUrl().path());
        d->streamingFormat = fi.completeSuffix().toLocal8Bit();
        QByteArray mimeType = getMimeType(d->streamingFormat);
        if (mimeType.isEmpty())
        {
            d->responseBody = QByteArray("Unsupported streaming format ") + mimeType;
            sendResponse("HTTP", CODE_INVALID_PARAMETER, "plain/text");
            return;
        }

        QnResourcePtr resource = qnResPool->getResourceByUniqId(fi.baseName());
        QnVideoCamera* camera = qnCameraPool->getVideoCamera(resource);
        if (!camera) {
            d->responseBody = "Media not found";
            sendResponse("HTTP", CODE_NOT_FOUND, "plain/text");
            return;
        }
        QnProgressiveDownloadingDataConsumer dataConsumer(this);
        dataProvider = camera->getLiveReader(QnResource::Role_LiveVideo);
        if (dataProvider == 0)
        {
            d->responseBody = "Video camera is not ready yet";
            sendResponse("HTTP", CODE_NOT_FOUND, "plain/text");
            return;
        }
        dataProvider->addDataProcessor(&dataConsumer);
        d->chunkedMode = true;
        sendResponse("HTTP", CODE_OK, mimeType);
        dataConsumer.sendResponse();
        QnByteArray emptyChunk(0,0);
        sendChunk(emptyChunk);
        dataProvider->removeDataProcessor(&dataConsumer);
    }

    d->socket->close();
    m_runing = false;
}

QnFfmpegTranscoder* QnProgressiveDownloadingConsumer::getTranscoder()
{
    Q_D(QnProgressiveDownloadingConsumer);
    return &d->transcoder;
}
