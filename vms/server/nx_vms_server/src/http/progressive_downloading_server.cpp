#include "progressive_downloading_server.h"

#include <memory>

#include <QtCore/QFileInfo>
#include <QtCore/QSettings>
#include <QtCore/QUrlQuery>

#include <server/server_globals.h>

#include <nx/utils/log/log.h>
#include <utils/common/util.h>
#include <nx/utils/string.h>
#include <nx/fusion/model_functions.h>
#include <utils/common/adaptive_sleep.h>
#include <utils/fs/file.h>
#include <network/tcp_connection_priv.h>
#include <network/tcp_listener.h>

#include "core/resource_management/resource_pool.h"
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include "nx/streaming/abstract_data_consumer.h"
#include "core/resource/camera_resource.h"

#include "nx/streaming/archive_stream_reader.h"
#include <nx/streaming/config.h>
#include "plugins/resource/server_archive/server_archive_delegate.h"

#include "transcoding/transcoder.h"
#include "transcoding/ffmpeg_transcoder.h"
#include "transcoding/ffmpeg_video_transcoder.h"
#include "camera/video_camera.h"
#include "camera/camera_pool.h"
#include "streaming/streaming_params.h"
#include "media_server/settings.h"
#include "cached_output_stream.h"
#include "common/common_module.h"
#include "audit/audit_manager.h"
#include <media_server/media_server_module.h>
#include "rest/server/json_rest_result.h"
#include <api/helpers/camera_id_helper.h>
#include <core/dataprovider/data_provider_factory.h>
#include <nx/metrics/metrics_storage.h>
#include <nx/utils/scope_guard.h>
#include <api/global_settings.h>
#include <export/sign_helper.h>

namespace {

static const int CONNECTION_TIMEOUT = 1000 * 5;
static const int MAX_QUEUE_SIZE = 30;
static const unsigned int DEFAULT_MAX_FRAMES_TO_CACHE_BEFORE_DROP = 1;

AVCodecID getPrefferedVideoCodec(const QByteArray& streamingFormat, const QString& defaultCodec)
{
    if (streamingFormat == "webm")
        return AV_CODEC_ID_VP8;
    if (streamingFormat == "mpjpeg")
        return AV_CODEC_ID_MJPEG;

    return findVideoEncoder(defaultCodec);
}

bool isCodecCompatibleWithFormat(AVCodecID codec, const QByteArray& streamingFormat)
{
    if (streamingFormat == "webm")
        return AV_CODEC_ID_VP8 == codec;
    if (streamingFormat == "mpjpeg")
        return AV_CODEC_ID_MJPEG == codec;

    if (streamingFormat == "mpegts"|| streamingFormat == "mp4" || streamingFormat == "ismv")
        return codec == AV_CODEC_ID_H264 || codec == AV_CODEC_ID_H265 || codec == AV_CODEC_ID_MPEG4;

    return false;
}

const std::optional<CameraMediaStreamInfo> findCompatibleStream(
    const std::vector<CameraMediaStreamInfo>& streams,
    const QByteArray& streamingFormat,
    const QString& requestedResolution)
{
    for (const auto& streamInfo: streams)
    {
        if (!streamInfo.transcodingRequired &&
            isCodecCompatibleWithFormat((AVCodecID)streamInfo.codec, streamingFormat) &&
            (requestedResolution.isEmpty() || requestedResolution == streamInfo.resolution))
        {
            return streamInfo;
        }
    }
    return std::nullopt;
}

}

// -------------------------- QnProgressiveDownloadingDataConsumer ---------------------

class QnProgressiveDownloadingDataConsumer: public QnAbstractDataConsumer
{
public:
    QnProgressiveDownloadingDataConsumer(
        QnProgressiveDownloadingConsumer* owner,
        bool standFrameDuration,
        bool dropLateFrames,
        unsigned int maxFramesToCacheBeforeDrop,
        bool liveMode,
        bool continuousTimestamps)
    :
        QnAbstractDataConsumer(50),
        m_owner(owner),
        m_standFrameDuration( standFrameDuration ),
        m_lastMediaTime(AV_NOPTS_VALUE),
        m_utcShift(0),
        m_maxFramesToCacheBeforeDrop( maxFramesToCacheBeforeDrop ),
        m_adaptiveSleep( MAX_FRAME_DURATION_MS*1000 ),
        m_rtStartTime( AV_NOPTS_VALUE ),
        m_lastRtTime( 0 ),
        m_endTimeUsec( AV_NOPTS_VALUE ),
        m_liveMode(liveMode),
        m_continuousTimestamps(continuousTimestamps),
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

    void setAuditHandle(const AuditHandle& handle) { m_auditHandle = handle; }

    void copyLastGopFromCamera(const QnVideoCameraPtr& camera)
    {
        camera->copyLastGop(
            nx::vms::api::StreamIndex::primary,
            /*skipTime*/ 0,
            m_dataQueue,
            /*iFramesOnly*/ false);
        m_dataQueue.setMaxSize(m_dataQueue.size() + MAX_QUEUE_SIZE);
    }

    void setEndTimeUsec(qint64 value) { m_endTimeUsec = value; }

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
        const QnAbstractMediaData* media = dynamic_cast<const QnAbstractMediaData*>(data.get());
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

        const QnAbstractMediaDataPtr& media = std::dynamic_pointer_cast<QnAbstractMediaData>(data);

        if (media->dataType == QnAbstractMediaData::EMPTY_DATA) {
            if (media->timestamp == DATETIME_NOW)
                m_needStop = true; // EOF reached
            finalizeMediaStream();
            return true;
        }

        if (m_endTimeUsec != AV_NOPTS_VALUE && media->timestamp > m_endTimeUsec)
        {
            m_needStop = true; // EOF reached
            finalizeMediaStream();
            return true;
        }

        if (media && m_auditHandle)
            m_owner->commonModule()->auditManager()->notifyPlaybackInProgress(m_auditHandle, media->timestamp);

        if (media && !(media->flags & QnAbstractMediaData::MediaFlags_LIVE) && m_continuousTimestamps)
        {
            if (m_lastMediaTime != (qint64)AV_NOPTS_VALUE && media->timestamp - m_lastMediaTime > MAX_FRAME_DURATION_MS*1000 &&
                media->timestamp != (qint64)AV_NOPTS_VALUE && media->timestamp != DATETIME_NOW)
            {
                m_utcShift -= (media->timestamp - m_lastMediaTime) - 1000000/60;
            }
            m_lastMediaTime = media->timestamp;
            media->timestamp += m_utcShift;
        }

        QnByteArray result(CL_MEDIA_ALIGNMENT, 0);
        bool isArchive = !(media->flags & QnAbstractMediaData::MediaFlags_LIVE);

        QnByteArray* const resultPtr =
            (m_dataOutput.get() &&
             !isArchive && // thin out only live frames
             m_dataOutput->packetsInQueue() > m_maxFramesToCacheBeforeDrop) ? NULL : &result;

        if( !resultPtr )
        {
            NX_VERBOSE(this, lit("Insufficient bandwidth to %1. Skipping frame...").
                arg(m_owner->getForeignAddress().toString()));
        }
        int errCode = m_owner->getTranscoder()->transcodePacket(
            media,
            resultPtr );   //if previous frame dispatch not even started, skipping current frame
        if( errCode == 0 )
        {
            if( resultPtr && result.size() > 0 )
                sendFrame(media->timestamp, result);
        }
        else
        {
            NX_DEBUG(this, lit("Terminating progressive download (url %1) connection from %2 due to transcode error (%3)").
                arg(m_owner->getDecodedUrl().toString()).arg(m_owner->getForeignAddress().toString()).arg(errCode));
            m_needStop = true;
        }

        return true;
    }

private:
    QnProgressiveDownloadingConsumer* m_owner;
    bool m_standFrameDuration;
    qint64 m_lastMediaTime;
    qint64 m_utcShift;
    std::unique_ptr<CachedOutputStream> m_dataOutput;
    const unsigned int m_maxFramesToCacheBeforeDrop;
    QnAdaptiveSleep m_adaptiveSleep;
    qint64 m_rtStartTime;
    qint64 m_lastRtTime;
    qint64 m_endTimeUsec;
    bool m_liveMode;
    bool m_continuousTimestamps;
    bool m_needKeyData;
    AuditHandle m_auditHandle;

    void sendFrame(qint64 timestamp, const QnByteArray& result)
    {
        //Preparing output packet. Have to do everything right here to avoid additional frame copying
        //TODO shared chunked buffer and socket::writev is wanted very much here
        QByteArray outPacket;
        const auto context = m_owner->getTranscoder()->getVideoCodecContext();
        if (context && context->codec_id == AV_CODEC_ID_MJPEG)
        {
            //preparing timestamp header
            QByteArray timestampHeader;

            // This is for buggy iOS 5, which for some reason stips multipart headers
            if (timestamp != AV_NOPTS_VALUE)
            {
                timestampHeader.append("Content-Type: image/jpeg;ts=");
                timestampHeader.append(QByteArray::number(timestamp, 10));
            }
            else
            {
                timestampHeader.append("Content-Type: image/jpeg");
            }
            timestampHeader.append("\r\n");

            if (timestamp != AV_NOPTS_VALUE)
            {
                timestampHeader.append("x-Content-Timestamp: ");
                timestampHeader.append(QByteArray::number(timestamp, 10));
                timestampHeader.append("\r\n");
            }

            //composing http chunk
            outPacket.reserve(result.size() + 12 + timestampHeader.size());    //12 - http chunk overhead
            outPacket.append(QByteArray::number((int)(result.size() + timestampHeader.size()), 16)); //http chunk
            outPacket.append("\r\n");                           //http chunk
                                                                //skipping delimiter
            const char* delimiterEndPos = (const char*)memchr(result.data(), '\n', result.size());
            if (delimiterEndPos == NULL)
            {
                outPacket.append(result.data(), result.size());
            }
            else
            {
                outPacket.append(result.data(), delimiterEndPos - result.data() + 1);
                outPacket.append(timestampHeader);
                outPacket.append(delimiterEndPos + 1, result.size() - (delimiterEndPos - result.data() + 1));
            }
            outPacket.append("\r\n");       //http chunk
        }
        else
        {
            outPacket.reserve(result.size() + 12);    //12 - http chunk overhead
            outPacket.append(QByteArray::number((int)result.size(), 16)); //http chunk
            outPacket.append("\r\n");                           //http chunk
            outPacket.append(result.data(), result.size());
            outPacket.append("\r\n");       //http chunk
        }

        //sending frame
        if (m_dataOutput.get())
        {   // Wait if bandwidth is not sufficient inside postPacket().
            // This is to ensure that we will send every archive packet.
            // This shouldn't affect live packets, as we thin them out above.
            // Refer to processData() for details.
            m_dataOutput->postPacket(outPacket, m_maxFramesToCacheBeforeDrop);
            if (m_dataOutput->failed())
                m_needStop = true;
        }
        else
        {
            if (!m_owner->sendBuffer(outPacket))
                m_needStop = true;
        }
    }

    QByteArray toHttpChunk( const char* data, size_t size )
    {
        QByteArray chunk;
        chunk.reserve((int) size + 12);
        chunk.append(QByteArray::number((int) size, 16));
        chunk.append("\r\n");
        chunk.append(data, (int) size);
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
            if( timeDiff <= MAX_FRAME_DURATION_MS*1000 )
                m_adaptiveSleep.terminatedSleep(timeDiff, MAX_FRAME_DURATION_MS*1000); // if diff too large, it is recording hole. do not calc delay for this case
        }
        m_lastRtTime = media->timestamp;
    }

    void finalizeMediaStream()
    {
        QnByteArray result(CL_MEDIA_ALIGNMENT, 0);
        if ((m_owner->getTranscoder()->finalize(&result) == 0) &&
            (result.size() > 0))
        {
            sendFrame(AV_NOPTS_VALUE, result);
        }
    }
};

// -------------- QnProgressiveDownloadingConsumer -------------------

class QnProgressiveDownloadingConsumerPrivate: public QnTCPConnectionProcessorPrivate
{
public:
    QnMediaServerModule* serverModule = nullptr;
    std::unique_ptr<QnFfmpegTranscoder> transcoder;
    QSharedPointer<QnArchiveStreamReader> archiveDP;
    //saving address and port in case socket will not make it to the destructor
    QString foreignAddress;
    unsigned short foreignPort = 0;
    bool terminated = false;
    quint64 killTimerID = 0;
    QnMutex mutex;
};

static QAtomicInt QnProgressiveDownloadingConsumer_count = 0;
static const QLatin1String DROP_LATE_FRAMES_PARAM_NAME( "dlf" );
static const QLatin1String STAND_FRAME_DURATION_PARAM_NAME( "sfd" );
static const QLatin1String RT_OPTIMIZATION_PARAM_NAME( "rt" ); // realtime transcode optimization
static const QLatin1String CONTINUOUS_TIMESTAMPS_PARAM_NAME( "ct" );
static const int MS_PER_SEC = 1000;

QnProgressiveDownloadingConsumer::QnProgressiveDownloadingConsumer(
    QnMediaServerModule* serverModule,
    std::unique_ptr<nx::network::AbstractStreamSocket> socket,
    QnTcpListener* owner)
    :
    QnTCPConnectionProcessor(new QnProgressiveDownloadingConsumerPrivate, std::move(socket), owner)
{
    Q_D(QnProgressiveDownloadingConsumer);
    d->serverModule = serverModule;

    d->socket->setRecvTimeout(CONNECTION_TIMEOUT);
    d->socket->setSendTimeout(CONNECTION_TIMEOUT);
    d->foreignAddress = d->socket->getForeignAddress().address.toString();
    d->foreignPort = d->socket->getForeignAddress().port;

    NX_DEBUG(this, lit("Established new progressive downloading session by %1:%2. Current session count %3").
        arg(d->foreignAddress).arg(d->foreignPort).
        arg(QnProgressiveDownloadingConsumer_count.fetchAndAddOrdered(1)+1));

    const int sessionLiveTimeoutSec =
        d->serverModule->settings().progressiveDownloadSessionLiveTimeSec();
    if( sessionLiveTimeoutSec > 0 )
        d->killTimerID = nx::utils::TimerManager::instance()->addTimer(
            this,
            std::chrono::milliseconds(sessionLiveTimeoutSec*MS_PER_SEC));

    setObjectName( "QnProgressiveDownloadingConsumer" );
}

QnProgressiveDownloadingConsumer::~QnProgressiveDownloadingConsumer()
{
    Q_D(QnProgressiveDownloadingConsumer);

    NX_DEBUG(this, lit("Progressive downloading session %1:%2 disconnected. Current session count %3").
        arg(d->foreignAddress).arg(d->foreignPort).
        arg(QnProgressiveDownloadingConsumer_count.fetchAndAddOrdered(-1)-1));

    quint64 killTimerID = 0;
    {
        QnMutexLocker lk( &d->mutex );
        killTimerID = d->killTimerID;
        d->killTimerID = 0;
    }
    if( killTimerID )
        nx::utils::TimerManager::instance()->joinAndDeleteTimer( killTimerID );

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
    else if (streamingFormat == "flv")
        return "video/x-flv";
    else if (streamingFormat == "f4v")
        return "video/x-f4v";
    else if (streamingFormat == "mpjpeg")
        return "multipart/x-mixed-replace;boundary=--ffserver";
    else
        return QByteArray();
}

void QnProgressiveDownloadingConsumer::sendMediaEventErrorResponse(
    Qn::MediaStreamEvent mediaStreamEvent)
{
    Q_D(QnProgressiveDownloadingConsumer);

    QnJsonRestResult error;
    error.errorString = toString(mediaStreamEvent);
    error.error = QnRestResult::Forbidden;
    d->response.messageBody = QJson::serialized(error);
    sendResponse(
        nx::network::http::StatusCode::ok,
        Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::JsonFormat));
}

void QnProgressiveDownloadingConsumer::sendJsonResponse(const QString& errorString)
{
    Q_D(QnProgressiveDownloadingConsumer);

    QnJsonRestResult error;
    error.errorString = errorString;
    error.error = QnRestResult::CantProcessRequest;
    d->response.messageBody = QJson::serialized(error);
    sendResponse(
        nx::network::http::StatusCode::ok,
        Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::JsonFormat));
}

void QnProgressiveDownloadingConsumer::run()
{
    Q_D(QnProgressiveDownloadingConsumer);
    initSystemThreadId();
    auto metrics = d->serverModule->commonModule()->metrics();

    std::atomic_int& donwloaderCount = metrics->tcpConnections().progressiveDownloading();
    auto newValue = ++donwloaderCount;
    auto metricsGuard = nx::utils::makeScopeGuard(
        [metrics]()
        {
            --metrics->tcpConnections().progressiveDownloading();
        });

    if (newValue > commonModule()->globalSettings()->maxWebMTranscoders())
    {
        sendMediaEventErrorResponse(Qn::MediaStreamEvent::TooManyOpenedConnections);
        return;
    }

    if (commonModule()->isTranscodeDisabled())
    {
        d->response.messageBody = QByteArray("Video transcoding is disabled in the server settings. Feature unavailable.");
        sendResponse(nx::network::http::StatusCode::notImplemented, "text/plain");
        return;
    }

    QnAbstractMediaStreamDataProviderPtr dataProvider;

    d->socket->setRecvTimeout(CONNECTION_TIMEOUT);
    d->socket->setSendTimeout(CONNECTION_TIMEOUT);

    bool ready = true;
    if (d->clientRequest.isEmpty())
        ready = readRequest();

    if (ready)
    {
        parseRequest();

        d->response.messageBody.clear();

        NX_DEBUG(this, "Start export data by url: [%1]", getDecodedUrl());

        //NOTE not using QFileInfo, because QFileInfo::completeSuffix returns suffix after FIRST '.'. So, unique ID cannot contain '.', but VMAX resource does contain
        const QString& requestedResourcePath = QnFile::fileName(getDecodedUrl().path());
        const int nameFormatSepPos = requestedResourcePath.lastIndexOf( QLatin1Char('.') );
        const QString& resId = requestedResourcePath.mid(0, nameFormatSepPos);
        QByteArray streamingFormat = nameFormatSepPos == -1 ? QByteArray()
            : requestedResourcePath.mid( nameFormatSepPos+1 ).toLatin1();
        QByteArray mimeType = getMimeType(streamingFormat);
        if (mimeType.isEmpty())
        {
            d->response.messageBody = QByteArray("Unsupported streaming format ") + mimeType;
            sendResponse(nx::network::http::StatusCode::notFound, "text/plain");
            return;
        }

        // If user request 'mp4' format use smooth streaming format to make mp4 streamable.
        if (streamingFormat == "mp4")
            streamingFormat = "ismv";

        const QUrlQuery decodedUrlQuery(getDecodedUrl().toQUrl());

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

        Qn::StreamQuality quality = Qn::StreamQuality::normal;
        if( decodedUrlQuery.hasQueryItem(QnCodecParams::quality) )
            quality = QnLexical::deserialized<Qn::StreamQuality>(decodedUrlQuery.queryItemValue(QnCodecParams::quality), Qn::StreamQuality::undefined);

        QnResourcePtr resource = nx::camera_id_helper::findCameraByFlexibleId(
            commonModule()->resourcePool(), resId);
        if (!resource)
        {
            d->response.messageBody = QByteArray("Resource with id ") + QByteArray(resId.toLatin1()) + QByteArray(" not found ");
            sendResponse(nx::network::http::StatusCode::notFound, "text/plain");
            return;
        }

        if (!resourceAccessManager()->hasPermission(d->accessRights, resource, Qn::ReadPermission))
        {
            sendUnauthorizedResponse(nx::network::http::StatusCode::forbidden);
            return;
        }

        Qn::MediaStreamEvent mediaStreamEvent = Qn::MediaStreamEvent::NoEvent;
        QnSecurityCamResourcePtr camRes = resource.dynamicCast<QnSecurityCamResource>();
        if (camRes)
            mediaStreamEvent = camRes->checkForErrors();
        if (mediaStreamEvent)
        {
            sendMediaEventErrorResponse(mediaStreamEvent);
            return;
        }

        QnFfmpegTranscoder::Config config;
        config.computeSignature = true;
        d->transcoder = std::make_unique<QnFfmpegTranscoder>(config, commonModule()->metrics());

        QnMediaResourcePtr mediaRes = resource.dynamicCast<QnMediaResource>();
        if (mediaRes)
        {
            QnLegacyTranscodingSettings extraParams;
            extraParams.resource = mediaRes;
            int rotation;
            if (decodedUrlQuery.hasQueryItem("rotation"))
                rotation = decodedUrlQuery.queryItemValue("rotation").toInt();
            else
                rotation = mediaRes->toResource()->getProperty(QnMediaResource::rotationKey()).toInt();
            extraParams.rotation = rotation;
            extraParams.forcedAspectRatio = mediaRes->customAspectRatio();

            d->transcoder->setTranscodingSettings(extraParams);
            d->transcoder->setStartTimeOffset(100 * 1000); // droid client has issue if enumerate timings from 0
        }

        const auto physicalResource = resource.dynamicCast<QnVirtualCameraResource>();
        if (!physicalResource)
        {
            d->response.messageBody = QByteArray("Transcoding error. Bad resource");
            NX_ERROR(this, d->response.messageBody);
            sendResponse(nx::network::http::StatusCode::internalServerError, "plain/text");
            return;
        }
        const auto requestedResolutionStr = resolutionStr.isEmpty() ?
            "" :
            QString("%1x%2").arg(videoSize.width()).arg(videoSize.height());

        AVCodecID videoCodec;
        QnTranscoder::TranscodeMethod transcodeMethod;
        QnServer::ChunksCatalog qualityToUse;
        auto streamInfo = findCompatibleStream(
            physicalResource->mediaStreams().streams, streamingFormat, requestedResolutionStr);
        if (streamInfo)
        {
            qualityToUse = streamInfo->getEncoderIndex() == nx::vms::api::StreamIndex::primary
                ? QnServer::HiQualityCatalog
                : QnServer::LowQualityCatalog;
            transcodeMethod = QnTranscoder::TM_DirectStreamCopy;
            videoCodec = (AVCodecID)streamInfo->codec;
        }
        else
        {
            qualityToUse = QnServer::HiQualityCatalog;
            transcodeMethod = QnTranscoder::TM_FfmpegTranscode;
            videoCodec = getPrefferedVideoCodec(
                streamingFormat, commonModule()->globalSettings()->defaultExportVideoCodec());
            NX_DEBUG(this,
                "Compatible video stream not found, transcoding will used, codec id[%1]",
                videoCodec);
        }

        if (d->transcoder->setVideoCodec(
                videoCodec,
                transcodeMethod,
                quality,
                videoSize,
                -1) != 0 )
        {
            d->response.messageBody = QByteArray("Transcoding error. Can not setup video codec:") +
                d->transcoder->getLastErrorMessage().toLatin1();
            NX_ERROR(this, d->response.messageBody);
            sendResponse(nx::network::http::StatusCode::internalServerError, "plain/text");
            return;
        }

        //taking max send queue size from url
        bool dropLateFrames = streamingFormat == "mpjpeg";
        unsigned int maxFramesToCacheBeforeDrop = DEFAULT_MAX_FRAMES_TO_CACHE_BEFORE_DROP;
        if( decodedUrlQuery.hasQueryItem(DROP_LATE_FRAMES_PARAM_NAME) )
        {
            maxFramesToCacheBeforeDrop = decodedUrlQuery.queryItemValue(DROP_LATE_FRAMES_PARAM_NAME).toUInt();
            dropLateFrames = true;
        }

        bool continuousTimestamps = decodedUrlQuery.queryItemValue(CONTINUOUS_TIMESTAMPS_PARAM_NAME) != lit("false");

        const bool standFrameDuration = decodedUrlQuery.hasQueryItem(STAND_FRAME_DURATION_PARAM_NAME);

        const bool rtOptimization = decodedUrlQuery.hasQueryItem(RT_OPTIMIZATION_PARAM_NAME);
        if (rtOptimization && d->serverModule->settings().ffmpegRealTimeOptimization())
            d->transcoder->setUseRealTimeOptimization(true);


        QByteArray position = decodedUrlQuery.queryItemValue( StreamingParams::START_POS_PARAM_NAME ).toLatin1();
        QByteArray endPosition = decodedUrlQuery.queryItemValue( StreamingParams::END_POS_PARAM_NAME ).toLatin1();
        bool isUTCRequest = !decodedUrlQuery.queryItemValue("posonly").isNull();
        auto camera = d->serverModule->videoCameraPool()->getVideoCamera(resource);

        bool isLive = position.isEmpty() || position == "now";
        auto requiredPermission = isLive
            ? Qn::Permission::ViewLivePermission : Qn::Permission::ViewFootagePermission;
        if (!commonModule()->resourceAccessManager()->hasPermission(d->accessRights, resource, requiredPermission))
        {
            if (commonModule()->resourceAccessManager()->hasPermission(
                d->accessRights, resource, Qn::Permission::ViewLivePermission))
            {
                sendMediaEventErrorResponse(Qn::MediaStreamEvent::ForbiddenWithNoLicense);
                return;
            }

            sendUnauthorizedResponse(nx::network::http::StatusCode::forbidden);
            return;
        }

        QnProgressiveDownloadingDataConsumer dataConsumer(
            this,
            standFrameDuration,
            dropLateFrames,
            maxFramesToCacheBeforeDrop,
            isLive,
            continuousTimestamps);

        qint64 timeUSec = DATETIME_NOW;
        if (isLive)
        {
            //if camera is offline trying to put it online

            if (isUTCRequest)
            {
                d->response.messageBody = "now";
                sendResponse(nx::network::http::StatusCode::ok, "text/plain");
                return;
            }

            if (!camera) {
                d->response.messageBody = "Media not found\r\n";
                sendResponse(nx::network::http::StatusCode::notFound, "text/plain");
                return;
            }
            QnLiveStreamProviderPtr liveReader = camera->getLiveReader(qualityToUse);
            dataProvider = liveReader;
            if (liveReader) {
                if (camera->isSomeActivity())
                    dataConsumer.copyLastGopFromCamera(camera); //< Don't copy deprecated gop if camera is not running now
                liveReader->startIfNotRunning();
                camera->inUse(this);
            }
        }
        else
        {
            bool utcFormatOK = false;
            timeUSec = nx::utils::parseDateTime( position );

            if (isUTCRequest)
            {
                QnAbstractArchiveDelegate* archive = 0;
                if (camRes)
                {
                    archive = camRes->createArchiveDelegate();
                    if (!archive)
                        archive = new QnServerArchiveDelegate(d->serverModule); // default value
                }
                if (archive)
                {
                    if (!archive->getFlags().testFlag(QnAbstractArchiveDelegate::Flag_CanSeekImmediatly))
                        archive->open(resource, d->serverModule->archiveIntegrityWatcher());
                    archive->seek( timeUSec, true);

                    if (auto mediaStreamEvent = archive->lastError().toMediaStreamEvent())
                    {
                        sendMediaEventErrorResponse(mediaStreamEvent);
                        return;
                    }

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
                    d->response.messageBody = callback + QByteArray("({'pos' : ") + ts + QByteArray("});");
                    sendResponse(nx::network::http::StatusCode::ok, "application/json");
                }
                else
                {
                    sendJsonResponse("Media archive is missing");
                }
                delete archive;
                return;
            }

            d->archiveDP = QSharedPointer<QnArchiveStreamReader> (dynamic_cast<QnArchiveStreamReader*> (
                d->serverModule->dataProviderFactory()->createDataProvider(resource, Qn::CR_Archive)));
            d->archiveDP->open(d->serverModule->archiveIntegrityWatcher());
            d->archiveDP->jumpTo(timeUSec, timeUSec);

            if (auto mediaEvent = d->archiveDP->lastError().toMediaStreamEvent())
            {
                sendMediaEventErrorResponse(mediaEvent);
                return;
            }


            if (!endPosition.isEmpty())
                dataConsumer.setEndTimeUsec(nx::utils::parseDateTime(endPosition));

            d->archiveDP->start();
            dataProvider = d->archiveDP;
        }

        if (dataProvider == 0)
        {
            sendJsonResponse("Video camera is not ready yet");
            return;
        }

        if (d->transcoder->setContainer(streamingFormat) != 0)
        {
            QByteArray msg;
            msg = QByteArray("Transcoding error. Can not setup output format:") +
                d->transcoder->getLastErrorMessage().toLatin1();
            sendJsonResponse(msg);
            return;
        }

        if (camRes && camRes->isAudioEnabled() && d->transcoder->isCodecSupported(AV_CODEC_ID_VORBIS))
            d->transcoder->setAudioCodec(AV_CODEC_ID_VORBIS, QnTranscoder::TM_FfmpegTranscode);

        dataProvider->addDataProcessor(&dataConsumer);
        d->chunkedMode = true;
        d->response.headers.insert( std::make_pair("Cache-Control", "no-cache") );
        sendResponse(nx::network::http::StatusCode::ok, mimeType);

        //dataConsumer.sendResponse();

        dataConsumer.setAuditHandle(commonModule()->auditManager()->notifyPlaybackStarted(authSession(), resource->getId(), timeUSec, false));

        dataConsumer.start();
        while( dataConsumer.isRunning() && d->socket->isConnected() && !d->terminated )
            readRequest(); // just reading socket to determine client connection is closed

        NX_DEBUG(this, "Done with progressive download connection from %1. Reason: %2",
            d->socket->getForeignAddress(),
            !dataConsumer.isRunning() ? lit("Data consumer stopped") :
                !d->socket->isConnected() ? lit("Connection has been closed") :
                    lit("Terminated")
        );

        dataConsumer.pleaseStop();

        auto signature = d->transcoder->getSignature(licensePool());
        sendChunk(QnSignHelper::buildSignatureFileEnd(signature));

        QnByteArray emptyChunk((unsigned)0,0);
        sendChunk(emptyChunk);
        sendData(QByteArray("\r\n"));

        dataProvider->removeDataProcessor(&dataConsumer);
        if (camera)
            camera->notInUse(this);
    }

    //d->socket->close();
}

void QnProgressiveDownloadingConsumer::onTimer( const quint64& /*timerID*/ )
{
    Q_D(QnProgressiveDownloadingConsumer);

    QnMutexLocker lk( &d->mutex );
    d->terminated = true;
    d->killTimerID = 0;
    pleaseStop();
}

QnFfmpegTranscoder* QnProgressiveDownloadingConsumer::getTranscoder()
{
    Q_D(QnProgressiveDownloadingConsumer);
    return d->transcoder.get();
}
