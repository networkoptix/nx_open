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
#include "plugins/resources/archive/archive_stream_reader.h"
#include "device_plugins/server_archive/server_archive_delegate.h"

static const int CONNECTION_TIMEOUT = 1000 * 5;
static const int MAX_QUEUE_SIZE = 10;

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
        run();
    }
    void copyLastGopFromCamera(QnVideoCamera* camera)
    {
        CLDataQueue tmpQueue(20);
        camera->copyLastGop(true, 0, tmpQueue);

        if (tmpQueue.size() > 0)
        {
            qint64 lastTime = tmpQueue.last()->timestamp;
            int timeResolution = (1000000ll / m_owner->getVideoStreamResolution());
            qint64 firstTime = lastTime - tmpQueue.size() * timeResolution;
            for (int i = 0; i < tmpQueue.size(); ++i)
            {
                QnAbstractMediaDataPtr srcMedia = qSharedPointerDynamicCast<QnAbstractMediaData>(tmpQueue.at(i));
                QnAbstractMediaDataPtr media = QnAbstractMediaDataPtr(srcMedia->clone());
                media->timestamp = firstTime + i*timeResolution;
                m_dataQueue.push(media);
            }
        }

        m_dataQueue.setMaxSize(m_dataQueue.size() + MAX_QUEUE_SIZE);
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
    QSharedPointer<QnArchiveStreamReader> archiveDP;
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

    d->socket->setReadTimeOut(CONNECTION_TIMEOUT);
    d->socket->setWriteTimeOut(CONNECTION_TIMEOUT);

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
            sendResponse("HTTP", CODE_NOT_FOUND, "text/plain");
            return;
        }

        QnResourcePtr resource = qnResPool->getResourceByUniqId(fi.baseName());
        if (resource == 0)
        {
            d->responseBody = QByteArray("Resource with unicId ") + QByteArray(fi.baseName().toLocal8Bit()) + QByteArray(" not found ");
            sendResponse("HTTP", CODE_NOT_FOUND, "text/plain");
            return;
        }

        QnProgressiveDownloadingDataConsumer dataConsumer(this);
        QByteArray position = getDecodedUrl().queryItemValue("pos").toLocal8Bit();
        bool isUTCRequest = !getDecodedUrl().queryItemValue("posonly").isNull();
        if (position.isEmpty() || position == "now")
        {
            if (isUTCRequest)
            {
                d->responseBody = "now";
                sendResponse("HTTP", CODE_OK, "text/plain");
                return;
            }

            QnVideoCamera* camera = qnCameraPool->getVideoCamera(resource);
            if (!camera) {
                d->responseBody = "Media not found";
                sendResponse("HTTP", CODE_NOT_FOUND, "text/plain");
                return;
            }
            dataProvider = camera->getLiveReader(QnResource::Role_LiveVideo);
            if (dataProvider)
                dataConsumer.copyLastGopFromCamera(camera);
        }
        else {
            bool utcFormatOK = false;
            qint64 timeMs = position.toLongLong(&utcFormatOK); // try UTC format
            if (!utcFormatOK)
                timeMs = QDateTime::fromString(position, Qt::ISODate).toMSecsSinceEpoch(); // try ISO format
            timeMs *= 1000;

            if (isUTCRequest)
            {
                QnServerArchiveDelegate serverArchive;
                serverArchive.open(resource);
                serverArchive.seek(timeMs, true);
                for (int i = 0; i < 20; ++i) 
                {
                    QnAbstractMediaDataPtr data = serverArchive.getNextData();
                    if (data->dataType == QnAbstractMediaData::VIDEO || data->dataType == QnAbstractMediaData::AUDIO)
                    {
                        if (utcFormatOK)
                            d->responseBody = QByteArray::number(data->timestamp/1000);
                        else
                            d->responseBody = QDateTime::fromMSecsSinceEpoch(data->timestamp/1000).toString(Qt::ISODate).toLocal8Bit();
                        sendResponse("HTTP", CODE_OK, "text/plain");
                        return;
                    }
                }
                d->responseBody = "Internal server error";
                sendResponse("HTTP", CODE_INTERNAL_ERROR, "text/plain");
                return;
            }

            d->archiveDP = QSharedPointer<QnArchiveStreamReader> (dynamic_cast<QnArchiveStreamReader*> (resource->createDataProvider(QnResource::Role_Archive)));
            d->archiveDP->open();
            d->archiveDP->jumpTo(timeMs, timeMs);
            d->archiveDP->start();
            dataProvider = d->archiveDP;
        }
        
        if (dataProvider == 0)
        {
            d->responseBody = "Video camera is not ready yet";
            sendResponse("HTTP", CODE_NOT_FOUND, "text/plain");
            return;
        }
        dataProvider->addDataProcessor(&dataConsumer);
        d->chunkedMode = true;
        d->responseHeaders.setValue("Cache-Control", "no-cache");
        sendResponse("HTTP", CODE_OK, mimeType);
        dataConsumer.sendResponse();
        QnByteArray emptyChunk(0,0);
        sendChunk(emptyChunk);
        dataProvider->removeDataProcessor(&dataConsumer);
    }

    d->socket->close();
    m_runing = false;
}

int QnProgressiveDownloadingConsumer::getVideoStreamResolution() const
{
    Q_D(const QnProgressiveDownloadingConsumer);
    if (d->streamingFormat == "webm")
        return 1000;
    else if (d->streamingFormat == "mpegts")
        return 90000;
    else 
        return 60;
}

QnFfmpegTranscoder* QnProgressiveDownloadingConsumer::getTranscoder()
{
    Q_D(QnProgressiveDownloadingConsumer);
    return &d->transcoder;
}
