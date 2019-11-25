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
#include <utils/fs/file.h>
#include <network/tcp_connection_priv.h>
#include <network/tcp_listener.h>

#include "core/resource_management/resource_pool.h"
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include "core/resource/camera_resource.h"

#include "nx/streaming/archive_stream_reader.h"
#include <nx/streaming/config.h>
#include "plugins/resource/server_archive/server_archive_delegate.h"

#include "transcoding/transcoder.h"
#include "transcoding/ffmpeg_transcoder.h"
#include "transcoding/ffmpeg_video_transcoder.h"
#include "camera/camera_pool.h"
#include "streaming/streaming_params.h"
#include "media_server/settings.h"
#include "common/common_module.h"
#include <media_server/media_server_module.h>
#include "rest/server/json_rest_result.h"
#include <api/helpers/camera_id_helper.h>
#include <core/dataprovider/data_provider_factory.h>
#include <nx/metrics/metrics_storage.h>
#include <nx/utils/scope_guard.h>
#include <api/global_settings.h>
#include <export/sign_helper.h>

#include "progressive_downloading_consumer.h"

namespace {

static const int CONNECTION_TIMEOUT = 1000 * 5;
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

// -------------- ProgressiveDownloadingServer -------------------

class ProgressiveDownloadingServerPrivate: public QnTCPConnectionProcessorPrivate
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

static QAtomicInt ProgressiveDownloadingServer_count = 0;
static const QLatin1String DROP_LATE_FRAMES_PARAM_NAME( "dlf" );
static const QLatin1String STAND_FRAME_DURATION_PARAM_NAME( "sfd" );
static const QLatin1String RT_OPTIMIZATION_PARAM_NAME( "rt" ); // realtime transcode optimization
static const QLatin1String CONTINUOUS_TIMESTAMPS_PARAM_NAME( "ct" );
static const int MS_PER_SEC = 1000;

ProgressiveDownloadingServer::ProgressiveDownloadingServer(
    QnMediaServerModule* serverModule,
    std::unique_ptr<nx::network::AbstractStreamSocket> socket,
    QnTcpListener* owner)
    :
    QnTCPConnectionProcessor(new ProgressiveDownloadingServerPrivate, std::move(socket), owner)
{
    Q_D(ProgressiveDownloadingServer);
    d->serverModule = serverModule;

    d->socket->setRecvTimeout(CONNECTION_TIMEOUT);
    d->socket->setSendTimeout(CONNECTION_TIMEOUT);
    d->foreignAddress = d->socket->getForeignAddress().address.toString();
    d->foreignPort = d->socket->getForeignAddress().port;

    NX_DEBUG(this, lit("Established new progressive downloading session by %1:%2. Current session count %3").
        arg(d->foreignAddress).arg(d->foreignPort).
        arg(ProgressiveDownloadingServer_count.fetchAndAddOrdered(1)+1));

    const int sessionLiveTimeoutSec =
        d->serverModule->settings().progressiveDownloadSessionLiveTimeSec();
    if( sessionLiveTimeoutSec > 0 )
        d->killTimerID = commonModule()->timerManager()->addTimer(
            this,
            std::chrono::milliseconds(sessionLiveTimeoutSec*MS_PER_SEC));

    setObjectName( "ProgressiveDownloadingServer" );
}

ProgressiveDownloadingServer::~ProgressiveDownloadingServer()
{
    Q_D(ProgressiveDownloadingServer);

    NX_DEBUG(this, lit("Progressive downloading session %1:%2 disconnected. Current session count %3").
        arg(d->foreignAddress).arg(d->foreignPort).
        arg(ProgressiveDownloadingServer_count.fetchAndAddOrdered(-1)-1));

    quint64 killTimerID = 0;
    {
        QnMutexLocker lk( &d->mutex );
        killTimerID = d->killTimerID;
        d->killTimerID = 0;
    }
    if( killTimerID )
        commonModule()->timerManager()->joinAndDeleteTimer( killTimerID );

    stop();
}

QByteArray ProgressiveDownloadingServer::getMimeType(const QByteArray& streamingFormat)
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

void ProgressiveDownloadingServer::sendMediaEventErrorResponse(
    Qn::MediaStreamEvent mediaStreamEvent)
{
    Q_D(ProgressiveDownloadingServer);

    QnJsonRestResult error;
    error.errorString = toString(mediaStreamEvent);
    error.error = QnRestResult::Forbidden;
    d->response.messageBody = QJson::serialized(error);
    sendResponse(
        nx::network::http::StatusCode::ok,
        Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::JsonFormat));
}

void ProgressiveDownloadingServer::sendJsonResponse(const QString& errorString)
{
    Q_D(ProgressiveDownloadingServer);

    QnJsonRestResult error;
    error.errorString = errorString;
    error.error = QnRestResult::CantProcessRequest;
    d->response.messageBody = QJson::serialized(error);
    sendResponse(
        nx::network::http::StatusCode::ok,
        Qn::serializationFormatToHttpContentType(Qn::SerializationFormat::JsonFormat));
}

void ProgressiveDownloadingServer::processPositionRequest(
    const QnResourcePtr& resource, qint64 timeUSec, const QByteArray& callback)
{
    Q_D(ProgressiveDownloadingServer);

    // live position
    if (timeUSec == DATETIME_NOW)
    {
        d->response.messageBody = "now";
        sendResponse(nx::network::http::StatusCode::ok, "text/plain");
        return;
    }

    // archive position
    std::unique_ptr<QnAbstractArchiveDelegate> archive;
    QnSecurityCamResourcePtr camRes = resource.dynamicCast<QnSecurityCamResource>();
    if (camRes)
    {
        archive.reset(camRes->createArchiveDelegate());
        if (!archive)
            archive = std::make_unique<QnServerArchiveDelegate>(d->serverModule); // default value
    }
    if (!archive)
    {
        sendJsonResponse("Media archive is missing");
        return;
    }
    if (!archive->getFlags().testFlag(QnAbstractArchiveDelegate::Flag_CanSeekImmediatly))
        archive->open(resource, d->serverModule->archiveIntegrityWatcher());

    archive->seek(timeUSec, true);

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
            if (data->dataType == QnAbstractMediaData::EMPTY_DATA && data->timestamp < DATETIME_NOW)
                continue; // ignore filler packet

            if (data->dataType == QnAbstractMediaData::VIDEO || data->dataType == QnAbstractMediaData::AUDIO)
            {
                timestamp = data->timestamp;
                break;
            }
        }
        counter++;
    }

    QByteArray ts("\"now\"");
    if (timestamp != (qint64)AV_NOPTS_VALUE)
    {
        ts = QByteArray("\"") +
            QDateTime::fromMSecsSinceEpoch(timestamp/1000).toString(Qt::ISODate).toLatin1() +
            QByteArray("\"");
    }
    d->response.messageBody = callback + QByteArray("({'pos' : ") + ts + QByteArray("});");
    sendResponse(nx::network::http::StatusCode::ok, "application/json");
    return;
}

void ProgressiveDownloadingServer::run()
{
    Q_D(ProgressiveDownloadingServer);
    initSystemThreadId();
    auto metrics = d->serverModule->commonModule()->metrics();
    metrics->tcpConnections().progressiveDownloading()++;
    QnTranscoder::TranscodeMethod transcodeMethod = QnTranscoder::TM_DirectStreamCopy;
    auto metricsGuard = nx::utils::makeScopeGuard(
        [metrics, &transcodeMethod]()
        {
            --metrics->tcpConnections().progressiveDownloading();
            if (transcodeMethod == QnTranscoder::TM_FfmpegTranscode)
                --metrics->progressiveDownloadingTranscoders();
        });
    if (commonModule()->isTranscodeDisabled())
    {
        d->response.messageBody = QByteArray(
            "Video transcoding is disabled in the server settings. Feature unavailable.");
        sendResponse(nx::network::http::StatusCode::notImplemented, "text/plain");
        return;
    }

    QnAbstractMediaStreamDataProviderPtr dataProvider;

    d->socket->setRecvTimeout(CONNECTION_TIMEOUT);
    d->socket->setSendTimeout(CONNECTION_TIMEOUT);

    if (d->clientRequest.isEmpty() && !readRequest())
    {
        //TODO why not send bad request?
        NX_WARNING(this, "Failed to read request");
        return;
    }
    parseRequest();

    d->response.messageBody.clear();

    NX_DEBUG(this, "Start export data by url: [%1]", getDecodedUrl());

    //NOTE not using QFileInfo, because QFileInfo::completeSuffix returns suffix after FIRST '.'.
    // So, unique ID cannot contain '.', but VMAX resource does contain
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
            NX_WARNING(this, "Invalid resolution specified for web streaming. Defaulting to 480p");
            videoSize = QSize(0,480);
        }
    }

    Qn::StreamQuality quality = Qn::StreamQuality::normal;
    if (decodedUrlQuery.hasQueryItem(QnCodecParams::quality))
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

    bool audioOnly = decodedUrlQuery.hasQueryItem("audio_only");

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
        transcodeMethod = QnTranscoder::TM_FfmpegTranscode;
        auto newValue = ++metrics->progressiveDownloadingTranscoders();
        if (newValue > commonModule()->globalSettings()->maxWebMTranscoders())
        {
            NX_DEBUG(this, "Close session, too many opened connections, max connections: %1",
                commonModule()->globalSettings()->maxWebMTranscoders());
            sendMediaEventErrorResponse(Qn::MediaStreamEvent::TooManyOpenedConnections);
            return;
        }
        qualityToUse = QnServer::HiQualityCatalog;
        videoCodec = getPrefferedVideoCodec(
            streamingFormat, commonModule()->globalSettings()->defaultExportVideoCodec());
        NX_DEBUG(this,
            "Compatible video stream not found, transcoding will used, codec id[%1]",
            videoCodec);
    }

    if (!audioOnly && d->transcoder->setVideoCodec(
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

    ProgressiveDownloadingConsumer::Config consumerConfig;
    consumerConfig.standFrameDuration = standFrameDuration;
    consumerConfig.dropLateFrames = dropLateFrames;
    consumerConfig.maxFramesToCacheBeforeDrop = maxFramesToCacheBeforeDrop;
    consumerConfig.liveMode = isLive;
    consumerConfig.continuousTimestamps = continuousTimestamps;
    consumerConfig.audioOnly = audioOnly;
    QByteArray endPosition =
        decodedUrlQuery.queryItemValue(StreamingParams::END_POS_PARAM_NAME).toLatin1();
    if (!endPosition.isEmpty())
        consumerConfig.endTimeUsec = nx::utils::parseDateTime(endPosition);
    ProgressiveDownloadingConsumer dataConsumer(this, consumerConfig);
    qint64 timeUSec = DATETIME_NOW;
    if (!isLive)
        timeUSec = nx::utils::parseDateTime(position);

    bool isUTCRequest = !decodedUrlQuery.queryItemValue("posonly").isNull();
    if (isUTCRequest)
    {
        // TODO do we need create transcoder for this request type?
        // TODO is this request type still needed?
        QByteArray callback = decodedUrlQuery.queryItemValue("callback").toLatin1();
        processPositionRequest(resource, timeUSec, callback);
        return;
    }
    auto camera = d->serverModule->videoCameraPool()->getVideoCamera(resource);
    if (isLive)
    {
        //if camera is offline trying to put it online
        if (!camera) {
            d->response.messageBody = "Media not found\r\n";
            sendResponse(nx::network::http::StatusCode::notFound, "text/plain");
            return;
        }
        QnLiveStreamProviderPtr liveReader = camera->getLiveReader(qualityToUse);
        dataProvider = liveReader;
        if (liveReader) {
            if (camera->isSomeActivity() && !audioOnly)
                dataConsumer.copyLastGopFromCamera(camera); //< Don't copy deprecated gop if camera is not running now
            liveReader->startIfNotRunning();
            camera->inUse(this);
        }
    }
    else
    {
        d->archiveDP = QSharedPointer<QnArchiveStreamReader> (dynamic_cast<QnArchiveStreamReader*> (
            d->serverModule->dataProviderFactory()->createDataProvider(resource, Qn::CR_Archive)));
        d->archiveDP->open(d->serverModule->archiveIntegrityWatcher());
        d->archiveDP->jumpTo(timeUSec, timeUSec);

        if (auto mediaEvent = d->archiveDP->lastError().toMediaStreamEvent())
        {
            sendMediaEventErrorResponse(mediaEvent);
            return;
        }

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

    if (audioOnly && camRes && !camRes->isAudioEnabled())
    {
        sendJsonResponse("Audio is disabled on camera, enable audio to get stream");
        return;
    }

    if (camRes && camRes->isAudioEnabled() && d->transcoder->isCodecSupported(AV_CODEC_ID_VORBIS))
        d->transcoder->setAudioCodec(AV_CODEC_ID_VORBIS, QnTranscoder::TM_FfmpegTranscode);

    dataProvider->addDataProcessor(&dataConsumer);
    d->chunkedMode = true;
    d->response.headers.insert( std::make_pair("Cache-Control", "no-cache") );
    sendResponse(nx::network::http::StatusCode::ok, mimeType);

    dataConsumer.setAuditHandle(commonModule()->auditManager()->notifyPlaybackStarted(authSession(), resource->getId(), timeUSec, false));
    dataConsumer.start();
    while( dataConsumer.isRunning() && d->socket->isConnected() && !d->terminated )
        readRequest(); // just reading socket to determine client connection is closed

    if (!d->socket->isConnected())
        metricsGuard.fire();

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

void ProgressiveDownloadingServer::onTimer(const quint64& /*timerID*/)
{
    Q_D(ProgressiveDownloadingServer);

    QnMutexLocker lk( &d->mutex );
    d->terminated = true;
    d->killTimerID = 0;
    pleaseStop();
}

QnFfmpegTranscoder* ProgressiveDownloadingServer::getTranscoder()
{
    Q_D(ProgressiveDownloadingServer);
    return d->transcoder.get();
}
