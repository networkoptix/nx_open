
#include <memory>

#include <QFileInfo>
#include <QSettings>
#include "progressive_downloading_server.h"
#include "utils/network/tcp_connection_priv.h"
#include "utils/network/tcp_listener.h"
#include "transcoding/transcoder.h"
#include "transcoding/ffmpeg_transcoder.h"
#include "camera/video_camera.h"
#include "core/resource_managment/resource_pool.h"
#include "camera/camera_pool.h"
#include "core/dataconsumer/abstract_data_consumer.h"
#include "plugins/resources/archive/archive_stream_reader.h"
#include "device_plugins/server_archive/server_archive_delegate.h"
#include "utils/common/util.h"
#include "core/resource/camera_resource.h"
#include "cached_output_stream.h"

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
    QnProgressiveDownloadingDataConsumer (QnProgressiveDownloadingConsumer* owner):
        QnAbstractDataConsumer(50),
        m_owner(owner),
        m_lastMediaTime(AV_NOPTS_VALUE),
        m_utcShift(0)
    {
        m_dataOutput.reset( new CachedOutputStream(owner) );
        m_dataOutput->start();
    }

    ~QnProgressiveDownloadingDataConsumer()
    {
        stop();
        m_dataOutput->stop();
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
        QnAbstractMediaDataPtr media = qSharedPointerDynamicCast<QnAbstractMediaData>(data);
        if (media && !(media->flags & QnAbstractMediaData::MediaFlags_LIVE))
        {
            if (m_lastMediaTime != (qint64)AV_NOPTS_VALUE && media->timestamp - m_lastMediaTime > MAX_FRAME_DURATION*1000 &&
                media->timestamp != (qint64)AV_NOPTS_VALUE && media->timestamp != DATETIME_NOW)
            {
                m_utcShift -= (media->timestamp - m_lastMediaTime) - 1000000/60;
            }
            m_lastMediaTime = media->timestamp;
            media->timestamp += m_utcShift;
        }

        QnByteArray result(CL_MEDIA_ALIGNMENT, 0);
        QnByteArray* const resultPtr = m_dataOutput->packetsInQueue() > 1 ? NULL : &result;
        if( !resultPtr )
        {
            NX_LOG( QString::fromLatin1("Insufficient bandwidth to %1:%2. Skipping frame...").
                arg(m_owner->socket()->getForeignAddress()).arg(m_owner->socket()->getForeignPort()), cl_logDEBUG2 );
        }
        int errCode = m_owner->getTranscoder()->transcodePacket(
            media,
            resultPtr );   //if previous frame dispatch not even started, skipping current frame
        if (errCode == 0) {
            if (resultPtr && result.size() > 0) {
                //if (!m_owner->sendChunk(result))
                //    m_needStop = true;
                m_dataOutput->postPacket(toHttpChunk(result.data(), result.size()));
                if( m_dataOutput->failed() )
                    m_needStop = true;
            }
        }
        else
            m_needStop = true;
        return true;
    }
private:
    QnProgressiveDownloadingConsumer* m_owner;
    qint64 m_lastMediaTime;
    qint64 m_utcShift;
    std::auto_ptr<CachedOutputStream> m_dataOutput;

    QByteArray toHttpChunk( const char* data, size_t size )
    {
        QByteArray chunk = QByteArray::number((int)size,16);
        chunk.append("\r\n");
        chunk.append(data, size);
        chunk.append("\r\n");
        return chunk;
    }
};

// -------------- QnProgressiveDownloadingConsumer -------------------

class QnProgressiveDownloadingConsumerPrivate: public QnTCPConnectionProcessorPrivate
{
public:
    QnFfmpegTranscoder transcoder;
    QByteArray streamingFormat;
    CodecID videoCodec;
    QSharedPointer<QnArchiveStreamReader> archiveDP;
    //saving address and port in case socket will not make it to the destructor
    QString foreignAddress;
    unsigned short foreignPort;
    bool terminated;
    quint64 killTimerID;
    QMutex mutex;

    QnProgressiveDownloadingConsumerPrivate()
    :
        videoCodec( CODEC_ID_NONE ),
        foreignPort( 0 ),
        terminated( false ),
        killTimerID( 0 )
    {
    }
};

static QAtomicInt QnProgressiveDownloadingConsumer_count = 0;
static const QLatin1String PROGRESSIVE_DOWNLOADING_SESSION_LIVE_TIME_PARAM_NAME("progressiveDownloading/sessionLiveTimeSec");
static int DEFAULT_MAX_CONNECTION_LIVE_TIME_MS = 30*60;    //30 minutes
static const int MS_PER_SEC = 1000;

extern QSettings qSettings;

QnProgressiveDownloadingConsumer::QnProgressiveDownloadingConsumer(TCPSocket* socket, QnTcpListener* _owner):
    QnTCPConnectionProcessor(new QnProgressiveDownloadingConsumerPrivate, socket, _owner)
{
    Q_D(QnProgressiveDownloadingConsumer);
    d->socketTimeout = CONNECTION_TIMEOUT;
    d->streamingFormat = "webm";
    d->videoCodec = CODEC_ID_VP8;

    d->foreignAddress = socket->getForeignAddress();
    d->foreignPort = socket->getForeignPort();

    NX_LOG( QString::fromLatin1("Established new progressive downloading session by %1:%2. Current session count %3").
        arg(d->foreignAddress).arg(d->foreignPort).
        arg(QnProgressiveDownloadingConsumer_count.fetchAndAddOrdered(1)+1), cl_logDEBUG1 );

    d->killTimerID = TimerManager::instance()->addTimer(
        this,
        qSettings.value( PROGRESSIVE_DOWNLOADING_SESSION_LIVE_TIME_PARAM_NAME, DEFAULT_MAX_CONNECTION_LIVE_TIME_MS ).toUInt()*MS_PER_SEC );
}

QnProgressiveDownloadingConsumer::~QnProgressiveDownloadingConsumer()
{
    Q_D(QnProgressiveDownloadingConsumer);

    NX_LOG( QString::fromLatin1("Progressive downloading session %1:%2 disconnected. Current session count %3").
        arg(d->foreignAddress).arg(d->foreignPort).
        arg(QnProgressiveDownloadingConsumer_count.fetchAndAddOrdered(-1)-1), cl_logDEBUG1 );

    quint64 killTimerID = 0;
    {
        QMutexLocker lk( &d->mutex );
        killTimerID = d->killTimerID;
        d->killTimerID = 0;
    }
    if( killTimerID )
        TimerManager::instance()->joinAndDeleteTimer( killTimerID );

    stop();
}

QByteArray QnProgressiveDownloadingConsumer::getMimeType(const QByteArray& streamingFormat)
{
    if (streamingFormat == "webm")
        return "video/webm";
    else if (streamingFormat == "mpegts")
        return "video/mp2t";
    else if (streamingFormat == "mp4")
        return "video/mp4";
    else if (streamingFormat == "3gp")
        return "video/3gp";
    else if (streamingFormat == "rtp")
        return "video/3gp";
    else if (streamingFormat == "mpjpeg")
        return "multipart/x-mixed-replace;boundary=--ffserver";
    else 
        return QByteArray();
}

void QnProgressiveDownloadingConsumer::updateCodecByFormat(const QByteArray& streamingFormat)
{
    Q_D(QnProgressiveDownloadingConsumer);

    if (streamingFormat == "webm")
        d->videoCodec = CODEC_ID_VP8;
    else if (streamingFormat == "mpegts")
        d->videoCodec = CODEC_ID_MPEG2VIDEO;
    else if (streamingFormat == "mp4")
        d->videoCodec = CODEC_ID_MPEG4;
    else if (streamingFormat == "3gp")
        d->videoCodec = CODEC_ID_MPEG4;
    else if (streamingFormat == "rtp")
        d->videoCodec = CODEC_ID_MPEG4;
    else if (streamingFormat == "mpjpeg")
        d->videoCodec = CODEC_ID_MJPEG;
}

void QnProgressiveDownloadingConsumer::run()
{
    Q_D(QnProgressiveDownloadingConsumer);

    QnAbstractMediaStreamDataProviderPtr dataProvider;

    d->socket->setReadTimeOut(CONNECTION_TIMEOUT);
    d->socket->setWriteTimeOut(CONNECTION_TIMEOUT);

    bool ready = true;
    if (d->clientRequest.isEmpty())
        ready = readRequest();

    if (ready)
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
        updateCodecByFormat(d->streamingFormat);

        QSize videoSize(640,480);
        QByteArray resolutionStr = getDecodedUrl().queryItemValue("resolution").toLocal8Bit().toLower();
        if (resolutionStr.endsWith('p'))
            resolutionStr = resolutionStr.left(resolutionStr.length()-1);
        QList<QByteArray> resolution = resolutionStr.split('x');
        if (resolution.size() == 1)
            resolution.insert(0,QByteArray("0"));
        if (resolution.size() == 2)
        {
            videoSize = QSize(resolution[0].trimmed().toInt(), resolution[1].trimmed().toInt());
            if ((videoSize.width() < 16 && videoSize.width() != 0) || videoSize.height() < 16)
            {
                qWarning() << "Invalid resolution specified for web streaming. Defaulting to 480p";
                videoSize = QSize(0,480);
            }
        }

        QnCodecTranscoder::Params codecParams;
        QList<QPair<QString, QString> > queryItems = getDecodedUrl().queryItems();
        for( QList<QPair<QString, QString> >::const_iterator
            it = queryItems.begin();
            it != queryItems.end();
            ++it )
        {
            codecParams[it->first] = it->second;
        }

        if (d->transcoder.setVideoCodec(d->videoCodec, QnTranscoder::TM_FfmpegTranscode, videoSize, -1, codecParams) != 0)
            //if (d->transcoder.setVideoCodec(CODEC_ID_MPEG2VIDEO, QnTranscoder::TM_FfmpegTranscode, QSize(640,480)) != 0)
        {
            QByteArray msg;
            msg = QByteArray("Transcoding error. Can not setup video codec:") + d->transcoder.getLastErrorMessage().toLocal8Bit();
            qWarning() << msg;
            d->responseBody = msg;
            sendResponse("HTTP", CODE_INTERNAL_ERROR, "plain/text");
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
        QnVideoCamera* camera = qnCameraPool->getVideoCamera(resource);

        //QnVirtualCameraResourcePtr camRes = resource.dynamicCast<QnVirtualCameraResource>();
        //if (camRes && camRes->isAudioEnabled())
        //    d->transcoder.setAudioCodec(CODEC_ID_VORBIS, QnTranscoder::TM_FfmpegTranscode);
        if (position.isEmpty() || position == "now")
        {
            if (resource->getStatus() != QnResource::Online && resource->getStatus() != QnResource::Recording)
            {
                d->responseBody = "Video camera is not ready yet";
                sendResponse("HTTP", CODE_NOT_FOUND, "text/plain");
                return;
            }

            if (isUTCRequest)
            {
                d->responseBody = "now";
                sendResponse("HTTP", CODE_OK, "text/plain");
                return;
            }

            if (!camera) {
                d->responseBody = "Media not found";
                sendResponse("HTTP", CODE_NOT_FOUND, "text/plain");
                return;
            }
            dataProvider = camera->getLiveReader(QnResource::Role_LiveVideo);
            if (dataProvider) {
                dataConsumer.copyLastGopFromCamera(camera);
                dataProvider->start();
                camera->inUse(this);
            }
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
                qint64 timestamp = AV_NOPTS_VALUE;
                for (int i = 0; i < 20; ++i) 
                {
                    QnAbstractMediaDataPtr data = serverArchive.getNextData();
                    if (data && (data->dataType == QnAbstractMediaData::VIDEO || data->dataType == QnAbstractMediaData::AUDIO))
                    {
                        timestamp = data->timestamp;
                        break;
                    }
                }

                QByteArray ts("\"now\"");
                QByteArray callback = getDecodedUrl().queryItemValue("callback").toLocal8Bit();
                if (timestamp != (qint64)AV_NOPTS_VALUE)
                {
                    if (utcFormatOK)
                        ts = QByteArray::number(timestamp/1000);
                    else
                        ts = QByteArray("\"") + QDateTime::fromMSecsSinceEpoch(timestamp/1000).toString(Qt::ISODate).toLocal8Bit() + QByteArray("\"");
                }
                d->responseBody = callback + QByteArray("({'pos' : ") + ts + QByteArray("});"); 
                sendResponse("HTTP", CODE_OK, "application/json");

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

        if (d->transcoder.setContainer(d->streamingFormat) != 0)
        {
            QByteArray msg;
            msg = QByteArray("Transcoding error. Can not setup output format:") + d->transcoder.getLastErrorMessage().toLocal8Bit();
            qWarning() << msg;
            d->responseBody = msg;
            sendResponse("HTTP", CODE_INTERNAL_ERROR, "plain/text");
            return;
        }

        dataProvider->addDataProcessor(&dataConsumer);
        d->chunkedMode = true;
        d->responseHeaders.setValue("Cache-Control", "no-cache");
        sendResponse("HTTP", CODE_OK, mimeType);

        //dataConsumer.sendResponse();
        dataConsumer.start();
        while (dataConsumer.isRunning() && d->socket->isConnected() && !d->terminated)
            readRequest(); // just reading socket to determine client connection is closed
        dataConsumer.pleaseStop();

        QnByteArray emptyChunk((unsigned)0,0);
        sendChunk(emptyChunk);
        dataProvider->removeDataProcessor(&dataConsumer);
        if (camera)
            camera->notInUse(this);
    }

    d->socket->close();
}

void QnProgressiveDownloadingConsumer::onTimer( const quint64& /*timerID*/ )
{
    Q_D(QnProgressiveDownloadingConsumer);

    QMutexLocker lk( &d->mutex );
    d->terminated = true;
    d->killTimerID = 0;
    pleaseStop();
}

int QnProgressiveDownloadingConsumer::getVideoStreamResolution() const
{
    Q_D(const QnProgressiveDownloadingConsumer);
    if (d->streamingFormat == "webm")
        return 1000;
    else if (d->streamingFormat == "mpegts")
        return 90000;
    else if (d->streamingFormat == "mp4")
        return 90000;
    else 
        return 60;
}

QnFfmpegTranscoder* QnProgressiveDownloadingConsumer::getTranscoder()
{
    Q_D(QnProgressiveDownloadingConsumer);
    return &d->transcoder;
}
