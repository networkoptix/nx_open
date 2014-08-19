#include "progressive_downloading_server.h"

#include <memory>

#include <QtCore/QFileInfo>
#include <QtCore/QSettings>
#include <QtCore/QUrlQuery>

#include <server/server_globals.h>

#include <utils/common/log.h>
#include "utils/common/util.h"
#include "utils/common/model_functions.h"
#include <utils/fs/file.h>
#include "utils/network/tcp_connection_priv.h"
#include "utils/network/tcp_listener.h"

#include "core/resource_management/resource_pool.h"
#include "core/dataconsumer/abstract_data_consumer.h"
#include "core/resource/camera_resource.h"

#include "plugins/resource/archive/archive_stream_reader.h"
#include "plugins/resource/server_archive/server_archive_delegate.h"

#include "transcoding/transcoder.h"
#include "transcoding/ffmpeg_transcoder.h"
#include "camera/video_camera.h"
#include "camera/camera_pool.h"
#include "network/authenticate_helper.h"

#include <media_server/settings.h>

#include "cached_output_stream.h"


static const int CONNECTION_TIMEOUT = 1000 * 5;
static const int MAX_QUEUE_SIZE = 30;

// -------------------------- QnProgressiveDownloadingDataConsumer ---------------------

static const unsigned int DEFAULT_MAX_FRAMES_TO_CACHE_BEFORE_DROP = 1;

class QnProgressiveDownloadingDataConsumer: public QnAbstractDataConsumer
{
public:
    QnProgressiveDownloadingDataConsumer(
        QnProgressiveDownloadingConsumer* owner,
        bool standFrameDuration,
        bool dropLateFrames,
        unsigned int maxFramesToCacheBeforeDrop,
        bool liveMode)
    :
        QnAbstractDataConsumer(50),
        m_owner(owner),
        m_standFrameDuration( standFrameDuration ),
        m_lastMediaTime(AV_NOPTS_VALUE),
        m_utcShift(0),
        m_maxFramesToCacheBeforeDrop( maxFramesToCacheBeforeDrop ),
        m_adaptiveSleep( MAX_FRAME_DURATION*1000 ),
        m_rtStartTime( AV_NOPTS_VALUE ),
        m_lastRtTime( 0 ),
        m_liveMode(liveMode),
        m_needKeyData(false)
    {
        if( dropLateFrames )
        {
            m_dataOutput.reset( new CachedOutputStream(owner) );
            m_dataOutput->start();
        }

        setObjectName( "QnProgressiveDownloadingDataConsumer" );
    }

    ~QnProgressiveDownloadingDataConsumer()
    {
        pleaseStop();
        m_adaptiveSleep.breakSleep();
        stop();
        if( m_dataOutput.get() )
            m_dataOutput->stop();
    }

    void copyLastGopFromCamera(QnVideoCamera* camera)
    {
        CLDataQueue tmpQueue(20);
        camera->copyLastGop(true, 0, tmpQueue, 0);

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

    virtual bool canAcceptData() const override
    {
        if (m_liveMode)
            return true;
        else 
            return QnAbstractDataConsumer::canAcceptData();
    }

    void putData(const QnAbstractDataPacketPtr& data) override
    {
        if (m_liveMode)
        {
            if (m_dataQueue.size() > m_dataQueue.maxSize())
            {
                m_needKeyData = true;
                return;
            }
        }
        const QnAbstractMediaData* media = dynamic_cast<const QnAbstractMediaData*>(data.data());
        if (m_needKeyData && media)
        {
            if (!(media->flags & AV_PKT_FLAG_KEY))
                return;
            m_needKeyData = false;
        }

        QnAbstractDataConsumer::putData(data);
    }


    virtual bool processData(const QnAbstractDataPacketPtr& data) override
    {
        if( m_standFrameDuration )
            doRealtimeDelay( data );


        const QnAbstractMediaDataPtr& media = qSharedPointerDynamicCast<QnAbstractMediaData>(data);

        if (media->dataType == QnAbstractMediaData::EMPTY_DATA) {
            if (media->timestamp == DATETIME_NOW)
                m_needStop = true; // EOF reached
            return true;
        }

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

        QnByteArray* const resultPtr = (m_dataOutput.get() && m_dataOutput->packetsInQueue() > m_maxFramesToCacheBeforeDrop) ? NULL : &result;
        if( !resultPtr )
        {
            NX_LOG( lit("Insufficient bandwidth to %1. Skipping frame...").
                arg(m_owner->socket()->getForeignAddress().toString()), cl_logDEBUG2 );
        }
        int errCode = m_owner->getTranscoder()->transcodePacket(
            media,
            resultPtr );   //if previous frame dispatch not even started, skipping current frame
        if( errCode == 0 )
        {
            if( resultPtr && result.size() > 0 )
            {
                //Preparing output packet. Have to do everything right here to avoid additional frame copying
                    //TODO shared chunked buffer and socket::writev is wanted very much here
                QByteArray outPacket;
                if( m_owner->getTranscoder()->getVideoCodecContext()->codec_id == CODEC_ID_MJPEG )
                {
                    //preparing timestamp header
                    QByteArray timestampHeader;

                    // This is for buggy iOS 5, which for some reason stips multipart headers
                    timestampHeader.append( "Content-Type: image/jpeg;ts=" );
                    timestampHeader.append( QByteArray::number(media->timestamp,10) );
                    timestampHeader.append( "\r\n" );

                    timestampHeader.append( "x-Content-Timestamp: " );
                    timestampHeader.append( QByteArray::number(media->timestamp,10) );
                    timestampHeader.append( "\r\n" );

                    //composing http chunk
                    outPacket.reserve(result.size() + 12 + timestampHeader.size());    //12 - http chunk overhead
                    outPacket.append(QByteArray::number((int)(result.size()+timestampHeader.size()),16)); //http chunk
                    outPacket.append("\r\n");                           //http chunk
                    //skipping delimiter
                    const char* delimiterEndPos = (const char*)memchr( result.data(), '\n', result.size() );
                    if( delimiterEndPos == NULL )
                    {
                        outPacket.append(result.data(), result.size());
                    }
                    else
                    {
                        outPacket.append( result.data(), delimiterEndPos-result.data()+1 );
                        outPacket.append( timestampHeader );
                        outPacket.append( delimiterEndPos+1, result.size() - (delimiterEndPos-result.data()+1) );
                    }
                    outPacket.append("\r\n");       //http chunk
                }
                else
                {
                    outPacket.reserve(result.size() + 12);    //12 - http chunk overhead
                    outPacket.append(QByteArray::number((int)result.size(),16)); //http chunk
                    outPacket.append("\r\n");                           //http chunk
                    outPacket.append(result.data(), result.size());
                    outPacket.append("\r\n");       //http chunk
                }

                //sending frame
                if( m_dataOutput.get() )
                {
                    //m_dataOutput->postPacket(toHttpChunk(result.data(), result.size()));
                    m_dataOutput->postPacket(outPacket);
                    if( m_dataOutput->failed() )
                        m_needStop = true;
                }
                else
                {
                    //if (!m_owner->sendChunk(result))
                    if( !m_owner->sendBuffer(outPacket) )
                        m_needStop = true;
                }
            }
        }
        else
        {
            NX_LOG( lit("Terminating progressive download (url %1) connection from %2 due to transcode error (%3)").
                arg(m_owner->getDecodedUrl().toString()).arg(m_owner->socket()->getForeignAddress().toString()).arg(errCode), cl_logDEBUG1 );
            m_needStop = true;
        }

        return true;
    }

private:
    QnProgressiveDownloadingConsumer* m_owner;
    bool m_standFrameDuration;
    qint64 m_lastMediaTime;
    qint64 m_utcShift;
    std::auto_ptr<CachedOutputStream> m_dataOutput;
    const unsigned int m_maxFramesToCacheBeforeDrop;
    QnAdaptiveSleep m_adaptiveSleep;
    qint64 m_rtStartTime;
    qint64 m_lastRtTime;
    bool m_liveMode;
    bool m_needKeyData;

    QByteArray toHttpChunk( const char* data, size_t size )
    {
        QByteArray chunk;
        chunk.reserve( size + 12 );
        chunk.append(QByteArray::number((int)size,16));
        chunk.append("\r\n");
        chunk.append(data, size);
        chunk.append("\r\n");
        return chunk;
    }

    void doRealtimeDelay( const QnAbstractDataPacketPtr& media )
    {
        if( m_rtStartTime == (qint64)AV_NOPTS_VALUE )
        {
            m_rtStartTime = media->timestamp;
        }
        else
        {
            qint64 timeDiff = media->timestamp - m_lastRtTime;
            if( timeDiff <= MAX_FRAME_DURATION*1000 )
                m_adaptiveSleep.terminatedSleep(timeDiff, MAX_FRAME_DURATION*1000); // if diff too large, it is recording hole. do not calc delay for this case
        }
        m_lastRtTime = media->timestamp;
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
static const QLatin1String DROP_LATE_FRAMES_PARAM_NAME( "dlf" );
static const QLatin1String STAND_FRAME_DURATION_PARAM_NAME( "sfd" );
static const int MS_PER_SEC = 1000;

QnProgressiveDownloadingConsumer::QnProgressiveDownloadingConsumer(QSharedPointer<AbstractStreamSocket> socket, QnTcpListener* _owner):
    QnTCPConnectionProcessor(new QnProgressiveDownloadingConsumerPrivate, socket)
{
    Q_UNUSED(_owner)
    Q_D(QnProgressiveDownloadingConsumer);
    d->socketTimeout = CONNECTION_TIMEOUT;
    d->streamingFormat = "webm";
    d->videoCodec = CODEC_ID_VP8;

    d->foreignAddress = socket->getForeignAddress().address.toString();
    d->foreignPort = socket->getForeignAddress().port;

    NX_LOG( lit("Established new progressive downloading session by %1:%2. Current session count %3").
        arg(d->foreignAddress).arg(d->foreignPort).
        arg(QnProgressiveDownloadingConsumer_count.fetchAndAddOrdered(1)+1), cl_logDEBUG1 );

    const int sessionLiveTimeoutSec = MSSettings::roSettings()->value(
        nx_ms_conf::PROGRESSIVE_DOWNLOADING_SESSION_LIVE_TIME,
        nx_ms_conf::DEFAULT_PROGRESSIVE_DOWNLOADING_SESSION_LIVE_TIME ).toUInt();
    if( sessionLiveTimeoutSec > 0 )
        d->killTimerID = TimerManager::instance()->addTimer(
            this,
            sessionLiveTimeoutSec*MS_PER_SEC );

    setObjectName( "QnProgressiveDownloadingConsumer" );
}

QnProgressiveDownloadingConsumer::~QnProgressiveDownloadingConsumer()
{
    Q_D(QnProgressiveDownloadingConsumer);

    NX_LOG( lit("Progressive downloading session %1:%2 disconnected. Current session count %3").
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
    initSystemThreadId();

    QnAbstractMediaStreamDataProviderPtr dataProvider;

    d->socket->setRecvTimeout(CONNECTION_TIMEOUT);
    d->socket->setSendTimeout(CONNECTION_TIMEOUT);

    bool ready = true;
    if (d->clientRequest.isEmpty())
        ready = readRequest();

    if (ready)
    {
        parseRequest();

        d->responseBody.clear();

        //NOTE not using QFileInfo, because QFileInfo::completeSuffix returns suffix after FIRST '.'. So, unique ID cannot contain '.', but VMAX resource does contain
        const QString& requestedResourcePath = QnFile::fileName(getDecodedUrl().path());
        const int nameFormatSepPos = requestedResourcePath.lastIndexOf( QLatin1Char('.') );
        const QString& resUniqueID = requestedResourcePath.mid(0, nameFormatSepPos);
        d->streamingFormat = nameFormatSepPos == -1 ? QByteArray() : requestedResourcePath.mid( nameFormatSepPos+1 ).toLatin1();
        QByteArray mimeType = getMimeType(d->streamingFormat);
        if (mimeType.isEmpty())
        {
            d->responseBody = QByteArray("Unsupported streaming format ") + mimeType;
            sendResponse(CODE_NOT_FOUND, "text/plain");
            return;
        }
        updateCodecByFormat(d->streamingFormat);

        const QUrlQuery decodedUrlQuery( getDecodedUrl() );

        QSize videoSize(640,480);
        QByteArray resolutionStr = decodedUrlQuery.queryItemValue("resolution").toLatin1().toLower();
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

        Qn::StreamQuality quality = Qn::QualityNormal;
        if( decodedUrlQuery.hasQueryItem(QnCodecParams::quality) )
            quality = QnLexical::deserialized<Qn::StreamQuality>(decodedUrlQuery.queryItemValue(QnCodecParams::quality), Qn::QualityNotDefined);

        QnCodecParams::Value codecParams;
        QList<QPair<QString, QString> > queryItems = decodedUrlQuery.queryItems();
        for( QList<QPair<QString, QString> >::const_iterator
            it = queryItems.begin();
            it != queryItems.end();
            ++it )
        {
            codecParams[it->first] = it->second;
        }

        QnResourcePtr resource = qnResPool->getResourceByUniqId(resUniqueID);
        if (resource == 0)
        {
            d->responseBody = QByteArray("Resource with unicId ") + QByteArray(resUniqueID.toLatin1()) + QByteArray(" not found ");
            sendResponse(CODE_NOT_FOUND, "text/plain");
            return;
        }

        QnMediaResourcePtr mediaRes = resource.dynamicCast<QnMediaResource>();
        if (mediaRes)
            d->transcoder.setVideoLayout(mediaRes->getVideoLayout());

        if (d->transcoder.setVideoCodec(
                d->videoCodec,
                QnTranscoder::TM_FfmpegTranscode,
                quality,
                videoSize,
                -1,
                codecParams ) != 0 )
        {
            QByteArray msg;
            msg = QByteArray("Transcoding error. Can not setup video codec:") + d->transcoder.getLastErrorMessage().toLatin1();
            qWarning() << msg;
            d->responseBody = msg;
            sendResponse(CODE_INTERNAL_ERROR, "plain/text");
            return;
        }


        //taking max send queue size from url
        bool dropLateFrames = d->streamingFormat == "mpjpeg";
        unsigned int maxFramesToCacheBeforeDrop = DEFAULT_MAX_FRAMES_TO_CACHE_BEFORE_DROP;
        if( decodedUrlQuery.hasQueryItem(DROP_LATE_FRAMES_PARAM_NAME) )
        {
            maxFramesToCacheBeforeDrop = decodedUrlQuery.queryItemValue(DROP_LATE_FRAMES_PARAM_NAME).toUInt();
            dropLateFrames = true;
        }

        const bool standFrameDuration = decodedUrlQuery.hasQueryItem(STAND_FRAME_DURATION_PARAM_NAME);

        QByteArray position = decodedUrlQuery.queryItemValue("pos").toLatin1();
        bool isUTCRequest = !decodedUrlQuery.queryItemValue("posonly").isNull();
        QnVideoCamera* camera = qnCameraPool->getVideoCamera(resource);

        //QnVirtualCameraResourcePtr camRes = resource.dynamicCast<QnVirtualCameraResource>();
        //if (camRes && camRes->isAudioEnabled())
        //    d->transcoder.setAudioCodec(CODEC_ID_VORBIS, QnTranscoder::TM_FfmpegTranscode);
        bool isLive = position.isEmpty() || position == "now";

        QnProgressiveDownloadingDataConsumer dataConsumer(
            this,
            standFrameDuration,
            dropLateFrames,
            maxFramesToCacheBeforeDrop,
            isLive);

        if (isLive)
        {
            if (resource->getStatus() != Qn::Online && resource->getStatus() != Qn::Recording)
            {
                d->responseBody = "Video camera is not ready yet";
                sendResponse(CODE_NOT_FOUND, "text/plain");
                return;
            }

            if (isUTCRequest)
            {
                d->responseBody = "now";
                sendResponse(CODE_OK, "text/plain");
                return;
            }

            if (!camera) {
                d->responseBody = "Media not found";
                sendResponse(CODE_NOT_FOUND, "text/plain");
                return;
            }
            QnLiveStreamProviderPtr liveReader = camera->getLiveReader(QnServer::HiQualityCatalog);
            dataProvider = liveReader;
            if (liveReader) {
                dataConsumer.copyLastGopFromCamera(camera);
                liveReader->startIfNotRunning();
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
                QnAbstractArchiveDelegate* archive = 0;
                QnSecurityCamResourcePtr camRes = resource.dynamicCast<QnSecurityCamResource>();
                if (camRes) {
                    archive = camRes->createArchiveDelegate();
                    if (!archive)
                        archive = new QnServerArchiveDelegate(); // default value
                }
                if (archive) {
                    archive->open(resource);
                    archive->seek(timeMs, true);
                    qint64 timestamp = AV_NOPTS_VALUE;
                    int counter = 0;
                    while (counter < 20)
                    {
                        const QnAbstractMediaDataPtr& data = archive->getNextData();
                        if (data)
                        {
                            if (data->dataType == QnAbstractMediaData::VIDEO || data->dataType == QnAbstractMediaData::AUDIO) 
                            {
                                timestamp = data->timestamp;
                                break;
                            }
                            else if (data->dataType == QnAbstractMediaData::EMPTY_DATA && data->timestamp < DATETIME_NOW)
                                continue; // ignore filler packet
                            counter++;
                        }
                        else {
                            counter++;
                        }
                    }

                    QByteArray ts("\"now\"");
                    QByteArray callback = decodedUrlQuery.queryItemValue("callback").toLatin1();
                    if (timestamp != (qint64)AV_NOPTS_VALUE)
                    {
                        if (utcFormatOK)
                            ts = QByteArray::number(timestamp/1000);
                        else
                            ts = QByteArray("\"") + QDateTime::fromMSecsSinceEpoch(timestamp/1000).toString(Qt::ISODate).toLatin1() + QByteArray("\"");
                    }
                    d->responseBody = callback + QByteArray("({'pos' : ") + ts + QByteArray("});"); 
                    sendResponse(CODE_OK, "application/json");
                }
                else {
                    sendResponse(CODE_INTERNAL_ERROR, "application/json");

                }
                delete archive;
                return;
            }

            d->archiveDP = QSharedPointer<QnArchiveStreamReader> (dynamic_cast<QnArchiveStreamReader*> (resource->createDataProvider(Qn::CR_Archive)));
            d->archiveDP->open();
            d->archiveDP->jumpTo(timeMs, timeMs);
            d->archiveDP->start();
            dataProvider = d->archiveDP;
        }
        
        if (dataProvider == 0)
        {
            d->responseBody = "Video camera is not ready yet";
            sendResponse(CODE_NOT_FOUND, "text/plain");
            return;
        }

        if (d->transcoder.setContainer(d->streamingFormat) != 0)
        {
            QByteArray msg;
            msg = QByteArray("Transcoding error. Can not setup output format:") + d->transcoder.getLastErrorMessage().toLatin1();
            qWarning() << msg;
            d->responseBody = msg;
            sendResponse(CODE_INTERNAL_ERROR, "plain/text");
            return;
        }

        dataProvider->addDataProcessor(&dataConsumer);
        d->chunkedMode = true;
        d->response.headers.insert( std::make_pair("Cache-Control", "no-cache") );
        sendResponse(CODE_OK, mimeType);

        //dataConsumer.sendResponse();
        dataConsumer.start();
        while( dataConsumer.isRunning() && d->socket->isConnected() && !d->terminated )
            readRequest(); // just reading socket to determine client connection is closed

        NX_LOG( lit("Done with progressive download (url %1) connection from %2. Reason: %3").
            arg(getDecodedUrl().toString()).arg(socket()->getForeignAddress().toString()).
            arg((!dataConsumer.isRunning() ? lit("Data consumer stopped") : 
                (!d->socket->isConnected() ? lit("Connection has been closed") :
                 lit("Terminated")))), cl_logDEBUG1 );

        dataConsumer.pleaseStop();

        QnByteArray emptyChunk((unsigned)0,0);
        sendChunk(emptyChunk);
        dataProvider->removeDataProcessor(&dataConsumer);
        if (camera)
            camera->notInUse(this);
    }

    //d->socket->close();
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
