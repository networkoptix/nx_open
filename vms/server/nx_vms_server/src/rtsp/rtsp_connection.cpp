#include "rtsp_connection.h"

#include <QtCore/QElapsedTimer>
#include <QtCore/QUrlQuery>
#include <nx/utils/uuid.h>
#include <QtCore/QSet>
#include <QtCore/QTextStream>
#include <QtCore/QBuffer>
#include <core/dataprovider/data_provider_factory.h>

extern "C"
{
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
};

#include <nx/streaming/abstract_data_consumer.h>
#include <utils/media/ffmpeg_helper.h>
#include <nx/utils/scope_guard.h>
#include <nx/streaming/abstract_media_stream_data_provider.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/resource_media_layout.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource/user_resource.h>
#include <nx/streaming/archive_stream_reader.h>
#include <nx/network/http/http_types.h>
#include <nx/utils/string.h>

#include <network/tcp_connection_priv.h>
#include <nx/network/rtsp/rtsp_types.h>
#include <nx/streaming/abstract_archive_delegate.h>
#include <camera/camera_pool.h>
#include <recorder/recording_manager.h>
#include <utils/common/util.h>
#include <rtsp/rtsp_data_consumer.h>
#include <plugins/resource/server_archive/server_archive_delegate.h>
#include <providers/live_stream_provider.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/camera_resource.h>
#include <core/resource/avi/thumbnails_stream_reader.h>
#include <rtsp/rtsp_ffmpeg_encoder.h>
#include <rtsp/rtp_universal_encoder.h>
#include <rtsp/rtsp_utils.h>
#include <utils/common/synctime.h>
#include <network/tcp_listener.h>
#include <media_server/settings.h>
#include <nx/fusion/model_functions.h>
#include <nx/network/http/custom_headers.h>
#include <audit/audit_manager.h>
#include <media_server/settings.h>
#include <streaming/streaming_params.h>
#include <media_server/settings.h>
#include <nx/utils/log/log.h>
#include <nx/utils/random.h>
#include <nx/fusion/serialization/lexical_enum.h>
#include <media_server/media_server_module.h>
#include <core/resource/avi/thumbnails_archive_delegate.h>
#include <api/helpers/camera_id_helper.h>
#include <api/global_settings.h>
#include <nx/metrics/metrics_storage.h>

class QnTcpListener;

using namespace nx::vms::api;

namespace {

static const QByteArray ENDL("\r\n");
static const std::chrono::hours kNativeRtspConnectionSendTimeout(1);
static const QByteArray kSendMotionHeaderName("x-send-motion");

}

// ------------- ServerTrackInfo --------------------

// ----------------------------- QnRtspConnectionProcessorPrivate ----------------------------

//static const int MAX_CAMERA_OPEN_TIME = 1000 * 5;
static const std::chrono::seconds kDefaultRtspTimeout(60);
const QString RTSP_CLOCK_FORMAT(QLatin1String("yyyyMMddThhmmssZ"));

QnMutex RtspServerTrackInfo::m_createSocketMutex;

namespace {

bool updatePort(nx::network::AbstractDatagramSocket* &socket, int port)
{
    delete socket;
    socket = nx::network::SocketFactory::createDatagramSocket().release();
    return socket->bind(nx::network::SocketAddress(nx::network::HostAddress::anyHost, port));
}

QByteArray getParamValue(const QByteArray& paramName, const QUrlQuery& urlQuery, const nx::network::http::HttpHeaders& headers)
{
    QByteArray paramValue = urlQuery.queryItemValue(paramName).toUtf8();
    if (paramValue.isEmpty())
        paramValue = nx::network::http::getHeaderValue(headers, QByteArray("x-") + paramName);
    return paramValue;
}

Qn::Permission requiredPermission(PlaybackMode mode)
{
    switch(mode)
    {
        case PlaybackMode::Archive:
        case PlaybackMode::ThumbNails:
            return Qn::ViewFootagePermission;
        case PlaybackMode::Export:
            return Qn::ExportPermission;
        default:
            break;
    }
    return Qn::ViewLivePermission;
}

} // namespace

bool RtspServerTrackInfo::openServerSocket(const QString& peerAddress)
{
    // try to find a couple of port, even for RTP, odd for RTCP
    QnMutexLocker lock( &m_createSocketMutex );
    mediaSocket = nx::network::SocketFactory::createDatagramSocket().release();
    rtcpSocket = nx::network::SocketFactory::createDatagramSocket().release();

    bool opened = mediaSocket->bind( nx::network::SocketAddress( nx::network::HostAddress::anyHost, 0 ) );
    if (!opened)
        return false;
    int startPort = mediaSocket->getLocalAddress().port;
    if(startPort&1)
        opened = updatePort(mediaSocket, ++startPort);
    if (opened)
        opened = rtcpSocket->bind( nx::network::SocketAddress( nx::network::HostAddress::anyHost, startPort+1 ) );

    while (!opened && startPort < 65534) {
        startPort+=2;
        opened = updatePort(mediaSocket,startPort) && updatePort(rtcpSocket,startPort+1);
    }

    if (opened)
    {
        mediaSocket->setDestAddr(peerAddress, clientPort);
        rtcpSocket->setDestAddr(peerAddress, clientRtcpPort);
        rtcpSocket->setNonBlockingMode(true);
    }
    return opened;
}

class QnRtspConnectionProcessorPrivate: public QnTCPConnectionProcessorPrivate
{
public:
    //enum State {State_Stopped, State_Paused, State_Playing, State_Rewind};

    QnRtspConnectionProcessorPrivate():
        QnTCPConnectionProcessorPrivate(),
        playbackMode(PlaybackMode::Live),
        lastMediaPacketTime(AV_NOPTS_VALUE),
        dataProcessor(0),
        sessionTimeOut(0),
        useProprietaryFormat(false),
        startTime(DATETIME_NOW), //< Default value
        startTimeTakenFromUrlQuery(false),
        endTime(0),
        rtspScale(1.0),
        lastPlayCSeq(0),
        quality(MEDIA_Quality_High),
        qualityFastSwitch(true),
        metadataChannelNum(7),
        audioEnabled(false),
        wasDualStreaming(false),
        wasCameraControlDisabled(false),
        tcpMode(true),
        peerHasAccess(true),
        serverModule(nullptr)
    {
    }

    QnAbstractMediaStreamDataProviderPtr getCurrentDP()
    {
        if (playbackMode == PlaybackMode::ThumbNails)
            return thumbnailsDP;
        else if (playbackMode == PlaybackMode::Live)
            return liveDpHi;
        else
            return archiveDP;
    }


    void deleteDP()
    {
        if (archiveDP)
            archiveDP->stop();
        if (thumbnailsDP)
            thumbnailsDP->stop();
        if (dataProcessor)
            dataProcessor->stop();

        if (liveDpHi)
            liveDpHi->removeDataProcessor(dataProcessor);
        if (liveDpLow)
            liveDpLow->removeDataProcessor(dataProcessor);
        if (archiveDP)
            archiveDP->removeDataProcessor(dataProcessor);
        if (thumbnailsDP)
            thumbnailsDP->removeDataProcessor(dataProcessor);
        archiveDP.clear();
        archiveDpOpened = false;
        delete dataProcessor;
        dataProcessor = 0;

        if (mediaRes) {
            auto camera = serverModule->videoCameraPool()->getVideoCamera(mediaRes->toResourcePtr());
            if (camera)
                camera->notInUse(this);
        }
    }

    ~QnRtspConnectionProcessorPrivate()
    {
        deleteDP();
    }

    QnLiveStreamProviderPtr liveDpHi;
    QnLiveStreamProviderPtr liveDpLow;
    QSharedPointer<QnArchiveStreamReader> archiveDP;
    bool archiveDpOpened = false;
    QSharedPointer<QnThumbnailsStreamReader> thumbnailsDP;
    PlaybackMode playbackMode;
    AuditHandle auditRecordHandle;
    QElapsedTimer lastReportTime;
    qint64 lastMediaPacketTime;

    QnRtspDataConsumer* dataProcessor;

    QString sessionId;
    int sessionTimeOut; // timeout in seconds. Not used if zerro
    QnMediaResourcePtr mediaRes;
    ServerTrackInfoMap trackInfo;
    bool useProprietaryFormat;
    bool multiChannelVideo = true;

    struct TranscodeParams
    {
        TranscodeParams(): resolution(640, 480), codecId(AV_CODEC_ID_NONE) {}

        bool isNull() const
        {
            bool isFilled = resolution.height() > 0 && codecId != AV_CODEC_ID_NONE;
            return !isFilled;
        }
        bool operator==(const TranscodeParams& other) const
        {
            return resolution == other.resolution &&
                codecId == other.codecId;
        }

        QSize resolution;
        enum AVCodecID codecId;
    };
    TranscodeParams transcodeParams;
    TranscodeParams prevTranscodeParams;

    qint64 startTime; // time from last range header
    bool startTimeTakenFromUrlQuery;
    qint64 endTime;   // time from last range header
    double rtspScale; // RTSP playing speed (1 - normal speed, 0 - pause, >1 fast forward, <-1 fast back e. t.c.)
    QnMutex mutex;
    int lastPlayCSeq;
    MediaQuality quality;
    bool qualityFastSwitch;
    int metadataChannelNum;
    bool audioEnabled;
    bool wasDualStreaming;
    bool wasCameraControlDisabled;
    bool tcpMode;
    bool peerHasAccess;
    QnMutex archiveDpMutex;
    QnMediaServerModule* serverModule = nullptr;
};

// ----------------------------- QnRtspConnectionProcessor ----------------------------

QnRtspConnectionProcessor::QnRtspConnectionProcessor(
    QnMediaServerModule* serverModule,
    std::unique_ptr<nx::network::AbstractStreamSocket> socket, QnTcpListener* owner)
    :
    QnTCPConnectionProcessor(new QnRtspConnectionProcessorPrivate, std::move(socket), owner)
{
    Q_D(QnRtspConnectionProcessor);
    d->serverModule = serverModule;
}

QnRtspConnectionProcessor::~QnRtspConnectionProcessor()
{
    Q_D(QnRtspConnectionProcessor);
    directDisconnectAll();
    if (d->auditRecordHandle && d->lastMediaPacketTime != AV_NOPTS_VALUE)
        auditManager()->notifyPlaybackInProgress(d->auditRecordHandle, d->lastMediaPacketTime);

    d->auditRecordHandle.reset();
    stop();
}

void QnRtspConnectionProcessor::notifyMediaRangeUsed(qint64 timestampUsec)
{
    Q_D(QnRtspConnectionProcessor);
    if (d->auditRecordHandle && d->lastReportTime.isValid() && d->lastReportTime.elapsed() >= QnAuditManager::MIN_PLAYBACK_TIME_TO_LOG)
    {
        auditManager()->notifyPlaybackInProgress(d->auditRecordHandle, timestampUsec);
        d->lastReportTime.restart();
    }
    d->lastMediaPacketTime = timestampUsec;
}

bool QnRtspConnectionProcessor::parseRequestParams()
{
    Q_D(QnRtspConnectionProcessor);
    QnTCPConnectionProcessor::parseRequest();

    nx::network::http::HttpHeaders::const_iterator scaleIter = d->request.headers.find("Scale");
    if( scaleIter != d->request.headers.end() )
        d->rtspScale = scaleIter->second.toDouble();

    nx::utils::Url url(d->request.requestLine.url);
    if (d->mediaRes == 0 && d->request.requestLine.url.path() != lit("*"))
    {
        QString resId = url.path();
        if (resId.startsWith('/'))
            resId = resId.mid(1);

        QnResourcePtr resource = nx::camera_id_helper::findCameraByFlexibleId(
            commonModule()->resourcePool(), resId);
        d->mediaRes = qSharedPointerDynamicCast<QnMediaResource>(resource);
    }

    if (!d->mediaRes)
    {
        d->response.messageBody = "Media resource not found";
        return false;
    }

    if (!nx::network::http::getHeaderValue(d->request.headers, Qn::EC2_INTERNAL_RTP_FORMAT).isNull())
    {
        d->useProprietaryFormat = true;
    }
    else
    {
        d->sessionTimeOut =
            std::chrono::duration_cast<std::chrono::seconds>(kDefaultRtspTimeout).count();
        d->socket->setRecvTimeout(d->sessionTimeOut * 1500);
    }
    const QUrlQuery urlQuery(url.query());
    d->transcodeParams.codecId = AV_CODEC_ID_NONE;
    QString codec = urlQuery.queryItemValue("codec");
    if (!codec.isEmpty())
    {
        d->transcodeParams.codecId = nx::rtsp::findEncoderCodecId(codec);
        if (d->transcodeParams.codecId == AV_CODEC_ID_NONE)
        {
            d->response.messageBody = "Requested codec is not supported: ";
            d->response.messageBody.append(codec);
            return false;
        }
    };

    const QString pos = urlQuery.queryItemValue(StreamingParams::START_POS_PARAM_NAME);
    if (!pos.isEmpty())
    {
        d->startTimeTakenFromUrlQuery = true;
        d->startTime = nx::utils::parseDateTime(pos);
    }
    else if (!d->startTimeTakenFromUrlQuery)
    {
        processRangeHeader();
    }


    d->transcodeParams.resolution = QSize();
    QByteArray resolutionStr = getParamValue("resolution", urlQuery, d->request.headers);
    if (!resolutionStr.isEmpty())
    {
        QSize videoSize(640,480);
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
                d->response.messageBody = "Invalid resolution specified: ";
                d->response.messageBody.append(resolutionStr);
                return false;
            }
        }
        d->transcodeParams.resolution = videoSize;
        if (d->transcodeParams.codecId == AV_CODEC_ID_NONE)
        {
            d->transcodeParams.codecId = findVideoEncoder(
                commonModule()->globalSettings()->defaultVideoCodec());
        }
    }

    QString qualityStr = nx::network::http::getHeaderValue(d->request.headers, "x-media-quality");
    if (!qualityStr.isEmpty())
    {
        d->quality = QnLexical::deserialized<MediaQuality>(qualityStr, MEDIA_Quality_High);
    }
    else
    {
        const QString& streamIndexStr = urlQuery.queryItemValue("stream");
        if( !streamIndexStr.isEmpty() )
        {
            const int streamIndex = streamIndexStr.toInt();
            if (streamIndex > 1)
            {
                d->response.messageBody = "Invalid stream specified: ";
                d->response.messageBody.append(streamIndexStr);
                return false;
            }
            d->quality = streamIndex == 0 ? MEDIA_Quality_High : MEDIA_Quality_Low;
        }
    }
    d->qualityFastSwitch = true;
    d->peerHasAccess = resourceAccessManager()->hasPermission(
        d->accessRights,
        d->mediaRes.dynamicCast<QnResource>(),
        requiredPermission(getStreamingMode()));

    d->clientRequest.clear();
    return true;
}

QnMediaResourcePtr QnRtspConnectionProcessor::getResource() const
{
    Q_D(const QnRtspConnectionProcessor);
    return d->mediaRes;

}

bool QnRtspConnectionProcessor::isLiveDP(QnAbstractStreamDataProvider* dp)
{
    Q_D(QnRtspConnectionProcessor);
    return dp == d->liveDpHi || dp == d->liveDpLow;
}

QHostAddress QnRtspConnectionProcessor::getPeerAddress() const
{
    Q_D(const QnRtspConnectionProcessor);
    return QHostAddress(d->socket->getForeignAddress().toString());
}

void QnRtspConnectionProcessor::initResponse(
    nx::network::rtsp::StatusCodeValue code,
    const QString& message)
{
    Q_D(QnRtspConnectionProcessor);

    d->response = nx::network::http::Response();
    d->response.statusLine.version = d->request.requestLine.version;
    d->response.statusLine.statusCode = static_cast<int>(code);
    d->response.statusLine.reasonPhrase = message.toUtf8();
    d->response.headers.insert(nx::network::http::HttpHeader("CSeq", nx::network::http::getHeaderValue(d->request.headers, "CSeq")));
    QString transport = nx::network::http::getHeaderValue(d->request.headers, "Transport");
    if (!transport.isEmpty())
        d->response.headers.insert(nx::network::http::HttpHeader("Transport", transport.toLatin1()));

    if (!d->sessionId.isEmpty()) {
        QString sessionId = d->sessionId;
        if (d->sessionTimeOut > 0)
            sessionId += QString(QLatin1String(";timeout=%1")).arg(d->sessionTimeOut);
        d->response.headers.insert(nx::network::http::HttpHeader("Session", sessionId.toLatin1()));
    }
}

void QnRtspConnectionProcessor::generateSessionId()
{
    Q_D(QnRtspConnectionProcessor);
    d->sessionId = QString::number(reinterpret_cast<uintptr_t>(d->socket.get()));
    d->sessionId += QString::number(nx::utils::random::number());
}


void QnRtspConnectionProcessor::sendResponse(
    nx::network::rtsp::StatusCodeValue statusCode,
    const QByteArray& contentType)
{
    Q_D(QnTCPConnectionProcessor);

    d->response.statusLine.version = d->request.requestLine.version;
    d->response.statusLine.statusCode = statusCode;
    d->response.statusLine.reasonPhrase = nx::network::rtsp::toString(statusCode).toUtf8();

    nx::network::http::insertOrReplaceHeader(
        &d->response.headers,
        nx::network::http::HttpHeader(nx::network::http::header::Server::NAME, nx::network::http::serverString()));
    nx::network::http::insertOrReplaceHeader(
        &d->response.headers,
        nx::network::http::HttpHeader("Date", nx::network::http::formatDateTime(QDateTime::currentDateTime())));

    if (!contentType.isEmpty())
        nx::network::http::insertOrReplaceHeader(
            &d->response.headers,
            nx::network::http::HttpHeader("Content-Type", contentType));
    if (!d->response.messageBody.isEmpty())
        nx::network::http::insertOrReplaceHeader(
            &d->response.headers,
            nx::network::http::HttpHeader(
                "Content-Length",
                QByteArray::number(d->response.messageBody.size())));

    const QByteArray response = d->response.toString();

    NX_DEBUG(this, "Server response to %1:\n%2",
        d->socket->getForeignAddress().address.toString(),
        response);

    NX_DEBUG(QnLog::HTTP_LOG_INDEX, "Sending response to %1:\n%2\n-------------------\n\n\n",
        d->socket->getForeignAddress().toString(),
        response);

    QnMutexLocker lock(&d->sockMutex);
    sendData(response.constData(), response.size());
}

int QnRtspConnectionProcessor::getMetadataChannelNum() const
{
    Q_D(const QnRtspConnectionProcessor);
    return d->metadataChannelNum;
}

int QnRtspConnectionProcessor::getAVTcpChannel(int trackNum) const
{
    Q_D(const QnRtspConnectionProcessor);
    ServerTrackInfoMap::const_iterator itr = d->trackInfo.find(trackNum);
    if (itr != d->trackInfo.end())
        return itr.value()->clientPort;
    else
        return -1;
}

RtspServerTrackInfo* QnRtspConnectionProcessor::getTrackInfo(int trackNum) const
{
    Q_D(const QnRtspConnectionProcessor);
    ServerTrackInfoMap::const_iterator itr = d->trackInfo.find(trackNum);
    if (itr != d->trackInfo.end())
        return itr.value().data();
    else
        return nullptr;
}

int QnRtspConnectionProcessor::getTracksCount() const
{
    Q_D(const QnRtspConnectionProcessor);
    return d->trackInfo.size();
}

int QnRtspConnectionProcessor::numOfVideoChannels()
{
    Q_D(QnRtspConnectionProcessor);
    if (!d->mediaRes)
        return -1;
    QnAbstractMediaStreamDataProviderPtr currentDP = d->getCurrentDP();

    QnConstResourceVideoLayoutPtr layout = d->mediaRes->getVideoLayout(currentDP.data());
    return layout ? layout->channelCount() : -1;
}

QByteArray QnRtspConnectionProcessor::getRangeStr()
{
    Q_D(QnRtspConnectionProcessor);
    QByteArray range;
    if (d->archiveDP)
    {
        if (!d->archiveDpOpened && !d->archiveDP->offlineRangeSupported())
            d->archiveDpOpened = d->archiveDP->open(d->serverModule->archiveIntegrityWatcher());

        qint64 archiveEndTime = d->archiveDP->endTime();
        bool endTimeIsNow = d->serverModule->recordingManager()->isCameraRecoring(d->archiveDP->getResource()); // && !endTimeInFuture;
        if (d->useProprietaryFormat)
        {
            // range in usecs since UTC
            range = "clock=";
            if (d->archiveDP->startTime() == (qint64)AV_NOPTS_VALUE)
                range += "now";
            else
                range += QByteArray::number(d->archiveDP->startTime());

            range += "-";
            if (endTimeIsNow)
                range += "now";
            else
                range += QByteArray::number(archiveEndTime);
        }
        else
        {
            // use 'clock' attrubute. see RFC 2326
            range = "clock=";
            if (d->archiveDP->startTime() == (qint64)AV_NOPTS_VALUE)
                range += QDateTime::currentDateTime().toUTC().toString(RTSP_CLOCK_FORMAT).toLatin1();
            else
                range += QDateTime::fromMSecsSinceEpoch(d->archiveDP->startTime()/1000).toUTC().toString(RTSP_CLOCK_FORMAT).toLatin1();
            range += "-";
            if (d->serverModule->recordingManager()->isCameraRecoring(d->archiveDP->getResource()))
                range += QDateTime::currentDateTime().toUTC().toString(RTSP_CLOCK_FORMAT);
            else
                range += QDateTime::fromMSecsSinceEpoch(d->archiveDP->endTime()/1000).toUTC().toString(RTSP_CLOCK_FORMAT).toLatin1();
        }
    }
    return range;
};

void QnRtspConnectionProcessor::addResponseRangeHeader()
{
    Q_D(QnRtspConnectionProcessor);

    QByteArray range = getRangeStr();
    if (!range.isEmpty())
    {
        nx::network::http::insertOrReplaceHeader(
            &d->response.headers, nx::network::http::HttpHeader("Range", range)
        );
    }
};

AbstractRtspEncoderPtr QnRtspConnectionProcessor::createEncoderByMediaData(
    QnConstAbstractMediaDataPtr mediaHigh,
    QnConstAbstractMediaDataPtr mediaLow,
    MediaQuality quality)
{
    QnConstAbstractMediaDataPtr media =
        quality == MEDIA_Quality_High || quality == MEDIA_Quality_ForceHigh
        ? mediaHigh
        : mediaLow;

    if (!media)
        return nullptr;

    Q_D(QnRtspConnectionProcessor);
    AVCodecID dstCodec = AV_CODEC_ID_NONE;
    if (media->dataType == QnAbstractMediaData::VIDEO)
        dstCodec = d->transcodeParams.codecId;
    else if (media->compressionType != AV_CODEC_ID_AAC && media->compressionType != AV_CODEC_ID_MP2)
        dstCodec = AV_CODEC_ID_MP2; // transcode all audio to mp2, excluded AAC and MP2

    if (commonModule()->isTranscodeDisabled() && dstCodec != AV_CODEC_ID_NONE)
    {
        NX_WARNING(this,
            "Video transcoding is disabled in the server settings. Feature unavailable.");
        return nullptr;
    }
    QnResourcePtr res = getResource()->toResourcePtr();
    nx::utils::Url url(d->request.requestLine.url);
    const QUrlQuery urlQuery( url.query() );
    int rotation;
    if (urlQuery.hasQueryItem("rotation"))
        rotation = urlQuery.queryItemValue("rotation").toInt();
    else
        rotation = res->getProperty(QnMediaResource::rotationKey()).toInt();

    QnUniversalRtpEncoder::Config config(DecoderConfig::fromResource(res));

    config.absoluteRtcpTimestamps = d->serverModule->settings().absoluteRtcpTimestamps();
    config.useRealTimeOptimization = d->serverModule->settings().ffmpegRealTimeOptimization();
    QString require = nx::network::http::getHeaderValue(d->request.headers, "Require");
    if (require.toLower().contains("onvif-replay"))
        config.addOnvifHeaderExtension = true;
    if (urlQuery.hasQueryItem("multiple_payload_types"))
        config.useMultipleSdpPayloadTypes = true;

    QnLegacyTranscodingSettings extraTranscodeParams;
    extraTranscodeParams.resource = getResource();
    extraTranscodeParams.rotation = rotation;
    extraTranscodeParams.forcedAspectRatio = getResource()->customAspectRatio();
    QnUniversalRtpEncoderPtr universalEncoder(new QnUniversalRtpEncoder(config, commonModule()->metrics()));
    if (!universalEncoder->open(
        mediaHigh, mediaLow, quality, dstCodec, d->transcodeParams.resolution, extraTranscodeParams))
    {
        NX_WARNING(this, "no RTSP encoder for codec %1 skip track", dstCodec);
        return nullptr;
    }
    return universalEncoder;
}

QnConstAbstractMediaDataPtr QnRtspConnectionProcessor::getCameraData(
    QnAbstractMediaData::DataType dataType, MediaQuality quality)
{
    Q_D(QnRtspConnectionProcessor);

    QnConstAbstractMediaDataPtr result;

    // 1. Check the packet in the GOP keeper.
    // Do not check the audio for the live point if the client is not proprietary.
    bool canCheckLive = (dataType == QnAbstractMediaData::VIDEO) || (d->startTime == DATETIME_NOW);
    if (canCheckLive)
    {
        QnVideoCameraPtr camera;
        const Qn::StreamIndex streamIndex =
            (quality == MEDIA_Quality_High || quality == MEDIA_Quality_ForceHigh)
            ? Qn::StreamIndex::primary
            : Qn::StreamIndex::secondary;
        if (getResource())
            camera = d->serverModule->videoCameraPool()->getVideoCamera(getResource()->toResourcePtr());

        if (camera)
        {
            if (dataType == QnAbstractMediaData::VIDEO)
                result =  camera->getLastVideoFrame(streamIndex, /*channel*/ 0);
            else
                result = camera->getLastAudioFrame(streamIndex);
            if (result)
                return result;
        }
    }

    // 2. Find a packet inside the archive.

    QnServerArchiveDelegate archive(d->serverModule, quality);
    if (!archive.open(getResource()->toResourcePtr(), d->serverModule->archiveIntegrityWatcher()))
        return result;
    if (d->startTime != DATETIME_NOW)
        archive.seek(d->startTime, true);
    if (archive.getAudioLayout()->channelCount() == 0 && dataType == QnAbstractMediaData::AUDIO)
        return result;

    for (int i = 0; i < 40; ++i)
    {
        QnConstAbstractMediaDataPtr media = archive.getNextData();
        if (!media)
            return result;
        if (media->dataType == dataType)
            return media;
    }
    return result;
}

QnConstMediaContextPtr QnRtspConnectionProcessor::getAudioCodecContext(int audioTrackIndex) const
{
    Q_D(const QnRtspConnectionProcessor);

    QnServerArchiveDelegate archive(d->serverModule);
    QnConstResourceAudioLayoutPtr layout;

    if (d->startTime == DATETIME_NOW)
    {
        layout = d->mediaRes->getAudioLayout(d->liveDpHi.data()); //< Layout from live video.
    }
    else if (archive.open(getResource()->toResourcePtr(), d->serverModule->archiveIntegrityWatcher()))
    {
        archive.seek(d->startTime, /*findIFrame*/ true);
        layout = archive.getAudioLayout(); //< Current position in archive.
    }

    if (layout && audioTrackIndex < layout->channelCount())
        return layout->getAudioTrackInfo(audioTrackIndex).codecContext;
    return QnConstMediaContextPtr(); //< Not found.
}

nx::network::rtsp::StatusCodeValue QnRtspConnectionProcessor::composeDescribe()
{
    Q_D(QnRtspConnectionProcessor);
    if (!d->mediaRes)
        return nx::network::http::StatusCode::notFound;

    // if transcoding used -> multiple channels will be sticked to single stream video
    if (d->transcodeParams.codecId != AV_CODEC_ID_NONE)
        d->multiChannelVideo = false;

    d->playbackMode = getStreamingMode();

    createDataProvider();

    QString acceptMethods = nx::network::http::getHeaderValue(d->request.headers, "Accept");
    if (acceptMethods.indexOf("sdp") == -1)
        return nx::network::http::StatusCode::notImplemented;

    QTextStream sdp(&d->response.messageBody);

    QnConstResourceVideoLayoutPtr videoLayout = d->mediaRes->getVideoLayout(d->liveDpHi.data());

    int numAudio = 0;
    QnVirtualCameraResourcePtr cameraResource = qSharedPointerDynamicCast<QnVirtualCameraResource>(d->mediaRes);
    if (cameraResource) {
        // avoid race condition if camera is starting now
        d->audioEnabled = cameraResource->isAudioEnabled();
        if (d->audioEnabled)
            numAudio = 1;
    }
    else {
        QnConstResourceAudioLayoutPtr audioLayout = d->mediaRes->getAudioLayout(d->liveDpHi.data());
        if (audioLayout)
            numAudio = audioLayout->channelCount();
    }

    int numVideo = 1;
    if (videoLayout && (d->useProprietaryFormat || d->multiChannelVideo))
        numVideo = videoLayout->channelCount();

    addResponseRangeHeader();

    nx::utils::Url controlUrl = d->request.requestLine.url;
    controlUrl.setQuery(QUrlQuery());
    nx::network::http::insertOrReplaceHeader(
        &d->response.headers,
        nx::network::http::HttpHeader("Content-Base", controlUrl.toString().toUtf8()));

    sdp << "v=0" << ENDL;
    sdp << "s=" << d->mediaRes->toResource()->getName() << ENDL;
    sdp << "c=IN IP4 " << d->socket->getLocalAddress().address.toString() << ENDL;
#if 0
    QUrl sessionControlUrl = d->request.requestLine.url;
    if( sessionControlUrl.port() == -1 )
        sessionControlUrl.setPort(d->serverModule->settings().port());
    sdp << "a=control:" << sessionControlUrl.toString() << ENDL;
#endif
    int i = 0;
    for (; i < numVideo + numAudio; ++i)
    {
        AbstractRtspEncoderPtr encoder;
        if (d->useProprietaryFormat)
        {
            bool isVideoTrack = (i < numVideo);
            QnRtspFfmpegEncoder* ffmpegEncoder = createRtspFfmpegEncoder(isVideoTrack);
            if (!isVideoTrack)
            {
                const int audioTrackIndex = i - numVideo;
                ffmpegEncoder->setCodecContext(getAudioCodecContext(audioTrackIndex));
            }
            encoder = AbstractRtspEncoderPtr(ffmpegEncoder);
        }
        else
        {
            QnAbstractMediaData::DataType dataType = i < numVideo ?
                QnAbstractMediaData::VIDEO : QnAbstractMediaData::AUDIO;
            QnConstAbstractMediaDataPtr mediaHigh = getCameraData(dataType, MEDIA_Quality_High);
            QnConstAbstractMediaDataPtr mediaLow = getCameraData(dataType, MEDIA_Quality_Low);
            encoder = createEncoderByMediaData(mediaHigh, mediaLow, d->quality);
            if (!encoder)
            {
                if (i >= numVideo)
                    continue; // if audio is not found do not report error. just skip track
            }
        }
        if (encoder == 0)
            return nx::network::http::StatusCode::notFound;

        sdp << encoder->getSdpMedia(i < numVideo, i);
        RtspServerTrackInfoPtr trackInfo(new RtspServerTrackInfo());
        trackInfo->setEncoder(std::move(encoder));
        d->trackInfo.insert(i, trackInfo);
    }

    if (d->playbackMode != PlaybackMode::ThumbNails && d->useProprietaryFormat)
    {
        RtspServerTrackInfoPtr trackInfo(new RtspServerTrackInfo());
        d->trackInfo.insert(d->metadataChannelNum, trackInfo);

        sdp << "m=metadata " << d->metadataChannelNum << " RTP/AVP " << RTP_METADATA_CODE << ENDL;
        sdp << "a=control:trackID=" << d->metadataChannelNum << ENDL;
        sdp << "a=rtpmap:" << RTP_METADATA_CODE << ' ' << RTP_METADATA_GENERIC_STR << "/" << CLOCK_FREQUENCY << ENDL;
    }
    return nx::network::http::StatusCode::ok;
}

int QnRtspConnectionProcessor::extractTrackId(const QString& path)
{
    int pos = path.lastIndexOf("/");
    QString trackStr = path.mid(pos+1);
    if (trackStr.toLower().startsWith("trackid"))
    {
        QStringList data = trackStr.split("=");
        if (data.size() > 1)
            return data[1].toInt();
    }
    return -1;
}

bool QnRtspConnectionProcessor::isSecondaryLiveDPSupported() const
{
    Q_D(const QnRtspConnectionProcessor);
    return d->liveDpLow && !d->liveDpLow->isPaused();
}

nx::network::rtsp::StatusCodeValue QnRtspConnectionProcessor::composeSetup()
{
    Q_D(QnRtspConnectionProcessor);
    if (!d->mediaRes)
        return nx::network::http::StatusCode::notFound;

    QByteArray transport = nx::network::http::getHeaderValue(d->request.headers, "Transport");
    QByteArray lowLevelTransport = "udp";
    if (transport.toLower().contains("tcp")) {
        lowLevelTransport = "tcp";
        d->tcpMode = true;
    }
    else {
        d->tcpMode = false;
    }

    int trackId = extractTrackId(d->request.requestLine.url.toString());

    QnAbstractMediaStreamDataProviderPtr currentDP = d->getCurrentDP();

    QnConstResourceVideoLayoutPtr videoLayout = d->mediaRes->getVideoLayout(currentDP.data());
    if (trackId >= videoLayout->channelCount()) {
        //QnAbstractMediaStreamDataProvider* dataProvider;
        if (d->archiveDP)
            d->archiveDP->setAudioChannel(trackId - videoLayout->channelCount());
    }

    if (trackId >= 0)
    {
        QList<QByteArray> transportInfo = transport.split(';');
        for(const QByteArray& data: transportInfo)
        {
            /*
            if (data.startsWith("interleaved="))
            {
                QList<QByteArray> ports = data.mid(QString("interleaved=").length()).split('-');
                d->trackPorts.insert(trackId, QPair<int,int>(ports[0].toInt(), ports.size() > 1 ? ports[1].toInt() : 0));
            }
            */
            if (data.startsWith("interleaved=") || data.startsWith("client_port="))
            {
                ServerTrackInfoMap::iterator itr = d->trackInfo.find(trackId);
                if (itr != d->trackInfo.end())
                {
                    RtspServerTrackInfoPtr trackInfo = itr.value();
                    QList<QByteArray> ports = data.split('=').last().split('-');
                    trackInfo->clientPort = ports[0].toInt();
                    trackInfo->clientRtcpPort = ports[1].toInt();
                    if (!d->tcpMode) {
                        if (trackInfo->openServerSocket(d->socket->getForeignAddress().address.toString())) {
                            transport.append(";server_port=").append(QByteArray::number(trackInfo->mediaSocket->getLocalAddress().port));
                            transport.append("-").append(QByteArray::number(trackInfo->rtcpSocket->getLocalAddress().port));
                        }
                    }
                }
            }
        }
    }
    nx::network::http::insertOrReplaceHeader(
        &d->response.headers,
        nx::network::http::HttpHeader("Transport", transport));
    return nx::network::http::StatusCode::ok;
}

nx::network::rtsp::StatusCodeValue QnRtspConnectionProcessor::composePause()
{
    //Q_D(QnRtspConnectionProcessor);
    //if (!d->dataProvider)
    //    return nx::network::http::StatusCode::notFound;

    //if (d->archiveDP)
    //    d->archiveDP->setSingleShotMode(true);

    //d->playTime += d->rtspScale * d->playTimer.elapsed()*1000;
    //d->rtspScale = 0;

    //d->state = QnRtspConnectionProcessorPrivate::State_Paused;
    return nx::network::http::StatusCode::ok;
}

void QnRtspConnectionProcessor::setRtspTime(qint64 time)
{
    Q_D(QnRtspConnectionProcessor);
    return d->dataProcessor->setLastSendTime(time);
}

qint64 QnRtspConnectionProcessor::getRtspTime()
{
    Q_D(QnRtspConnectionProcessor);
    if (d->dataProcessor)
        return d->dataProcessor->getDisplayedTime();
    else
        return AV_NOPTS_VALUE;
}

void QnRtspConnectionProcessor::processRangeHeader()
{
    Q_D(QnRtspConnectionProcessor);
    const auto rangeStr = nx::network::http::getHeaderValue(d->request.headers, "Range");
    if (!rangeStr.isEmpty())
    {
        QnVirtualCameraResourcePtr cameraResource = qSharedPointerDynamicCast<QnVirtualCameraResource>(d->mediaRes);
        if (!nx::network::rtsp::parseRangeHeader(rangeStr, &d->startTime, &d->endTime))
            d->startTime = DATETIME_NOW;

        if (rangeStr.startsWith("npt=") && d->startTime == 0)
            d->startTime = DATETIME_NOW; //< VLC/ffmpeg sends position 0 as a default value. Interpret it as Live position.
    }
}

void QnRtspConnectionProcessor::at_camera_resourceChanged(const QnResourcePtr & /*resource*/)
{
    Q_D(QnRtspConnectionProcessor);
    QnMutexLocker lock( &d->mutex );

    QnVirtualCameraResourcePtr cameraResource = qSharedPointerDynamicCast<QnVirtualCameraResource>(d->mediaRes);
    if (cameraResource) {
        if (cameraResource->isAudioEnabled() != d->audioEnabled ||
            cameraResource->hasDualStreaming() != d->wasDualStreaming ||
            (!cameraResource->isCameraControlDisabled() && d->wasCameraControlDisabled))
        {
            m_needStop = true;
            if (d->socket)
                d->socket->shutdown();
        }
    }
}

void QnRtspConnectionProcessor::at_camera_parentIdChanged(const QnResourcePtr & /*resource*/)
{
    Q_D(QnRtspConnectionProcessor);

    QnMutexLocker lock( &d->mutex );
    if (d->mediaRes && d->mediaRes->toResource()->hasFlags(Qn::foreigner))
    {
        m_needStop = true;
        if (d->socket)
            d->socket->shutdown();
    }
}

void QnRtspConnectionProcessor::createDataProvider()
{
    Q_D(QnRtspConnectionProcessor);

    const QUrlQuery urlQuery( d->request.requestLine.url.query() );
    QString speedStr = urlQuery.queryItemValue( "speed" );
    if( speedStr.isEmpty() )
        speedStr = nx::network::http::getHeaderValue( d->request.headers, "Speed" );

    const auto clientGuid = nx::network::http::getHeaderValue(d->request.headers, Qn::GUID_HEADER_NAME);

    if (!d->dataProcessor) {
        d->dataProcessor = new QnRtspDataConsumer(this);
        if (d->mediaRes)
            d->dataProcessor->setResource(d->mediaRes->toResourcePtr());
        d->dataProcessor->pauseNetwork();
        int speed = 1;  //real time
        if( d->useProprietaryFormat || speedStr.toLower() == "max")
        {
            speed = QnRtspDataConsumer::MAX_STREAMING_SPEED;
        }
        else if( !speedStr.isEmpty() )
        {
            bool ok = false;
            const int tmpSpeed = speedStr.toInt(&ok);
            if( ok && (tmpSpeed > 0) )
                speed = tmpSpeed;
        }
        d->dataProcessor->setStreamingSpeed(speed);
        d->dataProcessor->setMultiChannelVideo(d->useProprietaryFormat || d->multiChannelVideo);
    }
    else
        d->dataProcessor->clearUnprocessedData();

    QnVideoCameraPtr camera;
    if (d->mediaRes) {
        camera = d->serverModule->videoCameraPool()->getVideoCamera(d->mediaRes->toResourcePtr());
        QnNetworkResourcePtr cameraRes = d->mediaRes.dynamicCast<QnNetworkResource>();
        if (cameraRes && !cameraRes->isInitialized() && !cameraRes->hasFlags(Qn::foreigner))
            cameraRes->initAsync(true);
    }
    if (camera && d->playbackMode == PlaybackMode::Live)
    {
        if (!d->liveDpHi && !d->mediaRes->toResource()->hasFlags(Qn::foreigner) && d->mediaRes->toResource()->isInitialized()) {
            d->liveDpHi = camera->getLiveReader(QnServer::HiQualityCatalog);
            if (d->liveDpHi) {
                Qn::directConnect(
                    d->liveDpHi->getResource().data(),
                    &QnResource::parentIdChanged,
                    this,
                    &QnRtspConnectionProcessor::at_camera_parentIdChanged
                );

                Qn::directConnect(
                    d->liveDpHi->getResource().data(),
                    &QnResource::resourceChanged,
                    this,
                    &QnRtspConnectionProcessor::at_camera_resourceChanged
                );
            }
        }
        if (d->liveDpHi) {
            d->liveDpHi->addDataProcessor(d->dataProcessor);
            d->liveDpHi->startIfNotRunning();
        }

        if (!d->liveDpLow && d->liveDpHi)
        {
            QnVirtualCameraResourcePtr cameraRes = qSharedPointerDynamicCast<QnVirtualCameraResource> (d->mediaRes);
            QSharedPointer<QnLiveStreamProvider> liveHiProvider = qSharedPointerDynamicCast<QnLiveStreamProvider> (d->liveDpHi);
            int fps = d->liveDpHi->getLiveParams().fps;
            if (cameraRes->hasDualStreaming() && cameraRes->isEnoughFpsToRunSecondStream(fps))
                d->liveDpLow = camera->getLiveReader(QnServer::LowQualityCatalog);
        }
        if (d->liveDpLow) {
            d->liveDpLow->addDataProcessor(d->dataProcessor);
            d->liveDpLow->startIfNotRunning();
        }
    }
    if (!d->archiveDP)
    {
        QnMutexLocker lock(&d->archiveDpMutex);
        d->archiveDP = QSharedPointer<QnArchiveStreamReader>(
            dynamic_cast<QnArchiveStreamReader*>(d->serverModule->dataProviderFactory()->
                createDataProvider(
                    d->mediaRes->toResourcePtr(),
                    Qn::CR_Archive)));
        if (d->archiveDP)
        {
            d->archiveDP->setGroupId(clientGuid);
            d->archiveDP->getArchiveDelegate()->setPlaybackMode(d->playbackMode);
        }
    }

    if (!d->thumbnailsDP && d->playbackMode == PlaybackMode::ThumbNails)
    {
        QnSecurityCamResource* camRes = dynamic_cast<QnSecurityCamResource*>(d->mediaRes->toResource());
        QnAbstractArchiveDelegate* archiveDelegate = nullptr;
        if (camRes)
        {
            archiveDelegate = camRes->createArchiveDelegate();
            if (archiveDelegate &&
                !archiveDelegate->getFlags().testFlag(QnAbstractArchiveDelegate::Flag_CanProcessMediaStep))
            {
                archiveDelegate->setPlaybackMode(d->playbackMode);
                archiveDelegate = new QnThumbnailsArchiveDelegate(QnAbstractArchiveDelegatePtr(archiveDelegate));
            }
        }
        if (!archiveDelegate)
            archiveDelegate = new QnServerArchiveDelegate(d->serverModule); // default value
        archiveDelegate->setPlaybackMode(d->playbackMode);
        d->thumbnailsDP.reset(new QnThumbnailsStreamReader(d->mediaRes->toResourcePtr(), archiveDelegate));
        d->thumbnailsDP->setGroupId(clientGuid);
    }
}

void QnRtspConnectionProcessor::checkQuality()
{
    Q_D(QnRtspConnectionProcessor);
    if (d->liveDpHi &&
       (d->quality == MEDIA_Quality_Low || d->quality == MEDIA_Quality_LowIframesOnly))
    {
        if (d->liveDpLow == 0) {
            d->quality = MEDIA_Quality_High;
            NX_WARNING(this,
                "Low quality not supported for camera %1",
                d->mediaRes->toResource()->getUniqueId());
        }
        else if (d->liveDpLow->isPaused()) {
            d->quality = MEDIA_Quality_High;
            NX_WARNING(this,
                "Primary stream has big fps for camera %1 . Secondary stream is disabled.",
                d->mediaRes->toResource()->getUniqueId());
        }
    }
    QnVirtualCameraResourcePtr cameraRes = d->mediaRes.dynamicCast<QnVirtualCameraResource>();
    if (cameraRes) {
        d->wasDualStreaming = cameraRes->hasDualStreaming();
        d->wasCameraControlDisabled = cameraRes->isCameraControlDisabled();
    }
}

QnRtspFfmpegEncoder* QnRtspConnectionProcessor::createRtspFfmpegEncoder(bool isVideo)
{
    Q_D(const QnRtspConnectionProcessor);

    QnRtspFfmpegEncoder* result = new QnRtspFfmpegEncoder(DecoderConfig::fromMediaResource(d->mediaRes), commonModule()->metrics());
    if (isVideo && !d->transcodeParams.isNull())
        result->setDstResolution(d->transcodeParams.resolution, d->transcodeParams.codecId);
    return result;
}

void QnRtspConnectionProcessor::updatePredefinedTracks()
{
    Q_D(QnRtspConnectionProcessor);
    const bool isParamsEqual = d->transcodeParams == d->prevTranscodeParams;
    if (!isParamsEqual || d->startTime > 0)
    {
        // update encode resolution if position changed or other transcoding params
        for (const auto& track : d->trackInfo)
        {
            if (track->mediaType == RtspServerTrackInfo::MediaType::Video)
                track->setEncoder(AbstractRtspEncoderPtr(createRtspFfmpegEncoder(true)));
        }
        d->prevTranscodeParams = d->transcodeParams;
    }
}

void QnRtspConnectionProcessor::createPredefinedTracks(QnConstResourceVideoLayoutPtr videoLayout)
{
    Q_D(QnRtspConnectionProcessor);

    int trackNum = 0;
    for (; trackNum < videoLayout->channelCount(); ++trackNum)
    {
        RtspServerTrackInfoPtr vTrack(new RtspServerTrackInfo());
        vTrack->setEncoder(AbstractRtspEncoderPtr(createRtspFfmpegEncoder(true)));
        vTrack->clientPort = trackNum*2;
        vTrack->clientRtcpPort = trackNum*2 + 1;
        vTrack->mediaType = RtspServerTrackInfo::MediaType::Video;
        d->trackInfo.insert(trackNum, vTrack);
    }

    RtspServerTrackInfoPtr aTrack(new RtspServerTrackInfo());
    aTrack->setEncoder(AbstractRtspEncoderPtr(new QnRtspFfmpegEncoder(
        DecoderConfig::fromMediaResource(d->mediaRes),
        commonModule()->metrics())));
    aTrack->clientPort = trackNum*2;
    aTrack->clientRtcpPort = trackNum*2+1;
    d->trackInfo.insert(trackNum, aTrack);

    RtspServerTrackInfoPtr metaTrack(new RtspServerTrackInfo());
    metaTrack->clientPort = d->metadataChannelNum*2;
    metaTrack->clientRtcpPort = d->metadataChannelNum*2+1;
    d->trackInfo.insert(d->metadataChannelNum, metaTrack);

}

PlaybackMode QnRtspConnectionProcessor::getStreamingMode() const
{
    Q_D(const QnRtspConnectionProcessor);

    if (!nx::network::http::getHeaderValue(d->request.headers, "x-media-step").isEmpty())
        return PlaybackMode::ThumbNails;
    else if (d->rtspScale >= 0 && d->startTime == DATETIME_NOW)
        return PlaybackMode::Live;
    const bool isExport = nx::network::http::getHeaderValue(d->request.headers, Qn::EC2_MEDIA_ROLE) == "export";
    return isExport ? PlaybackMode::Export : PlaybackMode::Archive;
}

StreamDataFilters QnRtspConnectionProcessor::streamFilterFromHeaders(
    StreamDataFilters oldFilters) const
{
    Q_D(const QnRtspConnectionProcessor);
    QString deprecatedSendMotion = nx::network::http::getHeaderValue(
        d->request.headers, kSendMotionHeaderName);
    QString dataFilterStr = nx::network::http::getHeaderValue(
        d->request.headers, Qn::RTSP_DATA_FILTER_HEADER_NAME);

    StreamDataFilters filter = oldFilters ? oldFilters : StreamDataFilter::media;
    if (deprecatedSendMotion == "1" || deprecatedSendMotion == "true")
        filter.setFlag(StreamDataFilter::motion, true);
    else
        filter = QnLexical::deserialized<StreamDataFilters>(dataFilterStr);
    return filter;
}

nx::network::rtsp::StatusCodeValue QnRtspConnectionProcessor::composePlay()
{
    Q_D(QnRtspConnectionProcessor);
    if (d->mediaRes == 0)
        return nx::network::http::StatusCode::notFound;

    d->playbackMode = getStreamingMode();

    createDataProvider();
    checkQuality();

    if (d->trackInfo.isEmpty())
    {
        if (nx::network::http::getHeaderValue(d->request.headers, "x-play-now").isEmpty())
            return nx::network::http::StatusCode::internalServerError;
        d->useProprietaryFormat = true;
        d->sessionTimeOut = 0;
        d->socket->setSendTimeout(kNativeRtspConnectionSendTimeout); // set large timeout for native connection
        QnConstResourceVideoLayoutPtr videoLayout = d->mediaRes->getVideoLayout(d->liveDpHi.data());
        createPredefinedTracks(videoLayout);
        if (videoLayout) {
            QString layoutStr = videoLayout->toString();
            if (!layoutStr.isEmpty())
                d->response.headers.insert( std::make_pair(nx::network::http::StringType("x-video-layout"), layoutStr.toLatin1()) );
        }
    }
    else if (d->useProprietaryFormat)
    {
        updatePredefinedTracks();
    }

    d->lastPlayCSeq = nx::network::http::getHeaderValue(d->request.headers, "CSeq").toInt();


    QnVideoCameraPtr camera;
    if (d->mediaRes)
        camera = d->serverModule->videoCameraPool()->getVideoCamera(d->mediaRes->toResourcePtr());
    if (d->playbackMode == PlaybackMode::Live)
    {
        if (camera)
            camera->inUse(d);
        if (d->archiveDP)
            d->archiveDP->stop();
        if (d->thumbnailsDP)
            d->thumbnailsDP->stop();
    }
    else
    {
        if (camera)
            camera->notInUse(d);
        if (d->liveDpHi)
            d->liveDpHi->removeDataProcessor(d->dataProcessor);
        if (d->liveDpLow)
            d->liveDpLow->removeDataProcessor(d->dataProcessor);

        //if (d->liveMode == Mode_Archive && d->archiveDP)
        //    d->archiveDP->stop();
        //if (d->liveMode == Mode_ThumbNails && d->thumbnailsDP)
        //    d->thumbnailsDP->stop();
    }

    QnAbstractMediaStreamDataProviderPtr currentDP = d->getCurrentDP();

    if (d->useProprietaryFormat)
        addResponseRangeHeader();

    if (!currentDP)
        return nx::network::http::StatusCode::notFound;

    Qn::ResourceStatus status = getResource()->toResource()->getStatus();
    d->dataProcessor->setLiveMode(d->playbackMode == PlaybackMode::Live);
    if (d->playbackMode == PlaybackMode::Live)
    {
        auto camera = d->serverModule->videoCameraPool()->getVideoCamera(getResource()->toResourcePtr());
        if (!camera)
            return nx::network::http::StatusCode::notFound;

        QnMutexLocker dataQueueLock(d->dataProcessor->dataQueueMutex());
        int copySize = 0;
        if (!getResource()->toResource()->hasFlags(Qn::foreigner) && QnResource::isOnline(status))
        {
            const Qn::StreamIndex streamIndex =
                (d->quality != MEDIA_Quality_Low && d->quality != MEDIA_Quality_LowIframesOnly)
                ? Qn::StreamIndex::primary
                : Qn::StreamIndex::secondary;
            bool iFramesOnly = d->quality == MEDIA_Quality_LowIframesOnly;
            copySize = d->dataProcessor->copyLastGopFromCamera(
                camera,
                streamIndex,
                0, /* skipTime */
                iFramesOnly);
        }

        if (copySize == 0) {
            // no data from the camera. Insert several empty packets to inform client about it
            for (int i = 0; i < 3; ++i)
            {
                QnEmptyMediaDataPtr emptyData(new QnEmptyMediaData());
                emptyData->flags |= QnAbstractMediaData::MediaFlags_LIVE;
                if (i == 0)
                    emptyData->flags |= QnAbstractMediaData::MediaFlags_BOF;
                emptyData->timestamp = DATETIME_NOW;
                emptyData->opaque = d->lastPlayCSeq;

                d->dataProcessor->addData(emptyData);
            }
        }

        dataQueueLock.unlock();
        /**
         * Ignore rest of the packets (in case if the previous PLAY command was used to play
         * archive) before new position to make switch from archive to LIVE mode more quicker.
         */
        d->dataProcessor->setWaitCSeq(d->startTime, 0);
        d->dataProcessor->setLiveQuality(d->quality);
        d->dataProcessor->setLiveMarker(d->lastPlayCSeq);
    }
    else if ((d->playbackMode == PlaybackMode::Archive || d->playbackMode == PlaybackMode::Export)
              && d->archiveDP)
    {
        d->archiveDP->addDataProcessor(d->dataProcessor);

        d->archiveDP->lock();
        d->archiveDP->setStreamDataFilter(
            streamFilterFromHeaders(d->archiveDP->streamDataFilter()));

        d->archiveDP->setSpeed(d->rtspScale);
        d->archiveDP->setQuality(d->quality, d->qualityFastSwitch);
        if (d->startTime > 0)
        {
            d->dataProcessor->setSingleShotMode(d->startTime != DATETIME_NOW && d->startTime == d->endTime);
            d->dataProcessor->setWaitCSeq(d->startTime, d->lastPlayCSeq); // ignore rest packets before new position
            bool findIFrame = nx::network::http::getHeaderValue( d->request.headers, "x-no-find-iframe" ).isNull();
            d->archiveDP->setMarker(d->lastPlayCSeq);
            if (findIFrame)
                d->archiveDP->jumpTo(d->startTime, 0);
            else
                d->archiveDP->directJumpToNonKeyFrame(d->startTime);
        }
        else {
            d->archiveDP->setMarker(d->lastPlayCSeq);
        }
        d->archiveDP->unlock();

    }
    else if (d->playbackMode == PlaybackMode::ThumbNails && d->thumbnailsDP)
    {
        d->thumbnailsDP->addDataProcessor(d->dataProcessor);
        d->thumbnailsDP->setRange(d->startTime, d->endTime, nx::network::http::getHeaderValue(d->request.headers, "x-media-step").toLongLong(), d->lastPlayCSeq);
        d->thumbnailsDP->setQuality(d->quality);
    }

    d->dataProcessor->setUseUTCTime(d->useProprietaryFormat);
    d->dataProcessor->setStreamDataFilter(
        streamFilterFromHeaders(d->dataProcessor->streamDataFilter()));
    d->dataProcessor->start();


    if (!d->useProprietaryFormat)
    {
        //QString rtpInfo("url=%1;seq=%2;rtptime=%3");
        //d->responseHeaders.setValue("RTP-Info", rtpInfo.arg(d->requestHeaders.path()).arg(0).arg(0));
        QString rtpInfo("url=%1;seq=%2");
        d->response.headers.insert( std::make_pair("RTP-Info", rtpInfo.arg(d->request.requestLine.url.path()).arg(0).toLatin1()) );
    }

    if (currentDP)
        currentDP->start();
    if (d->playbackMode == PlaybackMode::Live && d->liveDpLow)
        d->liveDpLow->start();

    if (d->playbackMode != PlaybackMode::ThumbNails)
    {
        const qint64 startTimeUsec = d->playbackMode == PlaybackMode::Live ? DATETIME_NOW : d->startTime;
        bool isExport = d->playbackMode == PlaybackMode::Export;
        d->auditRecordHandle = auditManager()->notifyPlaybackStarted(authSession(), d->mediaRes->toResource()->getId(), startTimeUsec, isExport);
        d->lastReportTime.restart();
    }

    return nx::network::http::StatusCode::ok;
}

nx::network::rtsp::StatusCodeValue QnRtspConnectionProcessor::composeTeardown()
{
    Q_D(QnRtspConnectionProcessor);

    d->deleteDP();
    d->mediaRes = QnMediaResourcePtr(0);

    d->rtspScale = 1.0;
    d->startTime = d->endTime = 0;

    //d->encoders.clear();

    return nx::network::http::StatusCode::ok;
}

nx::network::rtsp::StatusCodeValue QnRtspConnectionProcessor::composeSetParameter()
{
    Q_D(QnRtspConnectionProcessor);

    if (!d->mediaRes)
        return nx::network::http::StatusCode::notFound;

    createDataProvider();

    QList<QByteArray> parameters = d->requestBody.split('\n');
    for(const QByteArray& parameter: parameters)
    {
        QByteArray normParam = parameter.trimmed().toLower();
        QList<QByteArray> vals = parameter.split(':');
        if (vals.size() < 2)
            return nx::network::http::StatusCode::badRequest;

        if (normParam.startsWith("x-media-quality"))
        {
            QString q = vals[1].trimmed();
            d->quality = QnLexical::deserialized<MediaQuality>(q, MEDIA_Quality_High);

            checkQuality();
            d->qualityFastSwitch = false;

            if (d->playbackMode == PlaybackMode::Live)
            {
                auto camera = d->serverModule->videoCameraPool()->getVideoCamera(getResource()->toResourcePtr());
                QnMutexLocker dataQueueLock(d->dataProcessor->dataQueueMutex());

                d->dataProcessor->setLiveQuality(d->quality);

                qint64 time = d->dataProcessor->lastQueuedTime();
                const Qn::StreamIndex streamIndex =
                    (d->quality != MEDIA_Quality_Low && d->quality != MEDIA_Quality_LowIframesOnly)
                    ? Qn::StreamIndex::primary
                    : Qn::StreamIndex::secondary;
                bool iFramesOnly = d->quality == MEDIA_Quality_LowIframesOnly;
                d->dataProcessor->copyLastGopFromCamera(
                    camera,
                    streamIndex,
                    time,
                    iFramesOnly); // for fast quality switching

                // set "main" dataProvider. RTSP data consumer is going to unsubscribe from other dataProvider
                // then it will be possible (I frame received)
            }
            d->archiveDP->setQuality(d->quality, d->qualityFastSwitch);
            return nx::network::http::StatusCode::ok;
        }
        else if (normParam.startsWith(kSendMotionHeaderName))
        {
            QByteArray value = vals[1].trimmed().toLower();
            const bool sendMotion = value == "1" || value == "true";

            StreamDataFilters filter = d->dataProcessor->streamDataFilter();
            filter.setFlag(StreamDataFilter::motion, sendMotion);

            if (d->archiveDP)
                d->archiveDP->setStreamDataFilter(filter);
            d->dataProcessor->setStreamDataFilter(filter);
            return nx::network::http::StatusCode::ok;
        }
        else if (normParam.startsWith(Qn::RTSP_DATA_FILTER_HEADER_NAME))
        {
            QByteArray value = vals[1].trimmed();
            StreamDataFilters filter = QnLexical::deserialized<StreamDataFilters>(value);
            if (d->archiveDP)
                d->archiveDP->setStreamDataFilter(filter);
            d->dataProcessor->setStreamDataFilter(filter);
            return nx::network::http::StatusCode::ok;
        }
    }

    return nx::network::http::StatusCode::badRequest;
}

nx::network::rtsp::StatusCodeValue QnRtspConnectionProcessor::composeGetParameter()
{
    Q_D(QnRtspConnectionProcessor);
    QList<QByteArray> parameters = d->requestBody.split('\n');
    for(const QByteArray& parameter: parameters)
    {
        QByteArray normParamName = parameter.trimmed().toLower();
        if (normParamName == "position" || normParamName.isEmpty())
        {
            d->response.messageBody.append("position: ");
            d->response.messageBody.append(QDateTime::fromMSecsSinceEpoch(getRtspTime()/1000).toUTC().toString(RTSP_CLOCK_FORMAT));
            d->response.messageBody.append(ENDL);
            addResponseRangeHeader();
        }
        else {
            NX_WARNING(this, "Unsupported RTSP parameter %1", parameter.trimmed());
            return nx::network::rtsp::StatusCode::parameterNotUnderstood;
        }
    }

    return nx::network::http::StatusCode::ok;
}

void QnRtspConnectionProcessor::processRequest()
{
    Q_D(QnRtspConnectionProcessor);
    QnMutexLocker lock( &d->mutex );

    if (d->dataProcessor)
        d->dataProcessor->pauseNetwork();

    QString method = d->request.requestLine.method;
    if (method != "OPTIONS" && d->sessionId.isEmpty())
        generateSessionId();
    int code = nx::network::http::StatusCode::ok;
    initResponse();
    QByteArray contentType;
    if (method == "OPTIONS")
    {
        d->response.headers.insert( std::make_pair("Public", "DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE, GET_PARAMETER, SET_PARAMETER") );
    }
    else if (method == "DESCRIBE")
    {
        code = composeDescribe();
        contentType = "application/sdp";
    }
    else if (method == "SETUP")
    {
        code = composeSetup();
    }
    else if (method == "PLAY")
    {
        code = composePlay();
    }
    else if (method == "PAUSE")
    {
        code = composePause();
    }
    else if (method == "TEARDOWN")
    {
        code = composeTeardown();
    }
    else if (method == "GET_PARAMETER")
    {
        code = composeGetParameter();
        contentType = "text/parameters";
    }
    else if (method == "SET_PARAMETER")
    {
        code = composeSetParameter();
        contentType = "text/parameters";
    }
    sendResponse(code, contentType);
    if (d->dataProcessor)
        d->dataProcessor->resumeNetwork();
}

void QnRtspConnectionProcessor::processBinaryRequest()
{

}

int QnRtspConnectionProcessor::isFullBinaryMessage(const QByteArray& data)
{
    if (data.size() < 4)
        return 0;
    int msgLen = (data.at(2) << 8) + data.at(3) + 4;
    if (data.size() < msgLen)
        return 0;
    return msgLen;
}

void QnRtspConnectionProcessor::run()
{
    Q_D(QnRtspConnectionProcessor);

    initSystemThreadId();

    //d->socket->setNoDelay(true);

    // NOTE: Sending data bigger than socket's send buffer size causes unexpected delays in some
    // scenarios. E.g., when streaming within a single Linux PC (VMS-11880).
    // Also, send buffer size cannot be set to a value less than 32K in Linux.
    d->socket->setSendBufferSize(64*1024);
    //d->socket->setRecvTimeout(1000*1000);
    //d->socket->setSendTimeout(1000*1000);

    if (d->clientRequest.isEmpty())
        readRequest();

    if (!parseRequestParams())
    {
        NX_WARNING(this,
            "RTSP request parsing error: %1, request:\n%2",
            d->response.messageBody,
            d->clientRequest);
        sendResponse(nx::network::http::StatusCode::badRequest, QByteArray());
        return;
    }

    if (!d->peerHasAccess)
    {
        sendUnauthorizedResponse(nx::network::http::StatusCode::forbidden);
        return;
    }

    processRequest();

    auto guard = nx::utils::makeScopeGuard(
        [d]()
        {
            d->deleteDP();
            d->socket->close();
            d->trackInfo.clear();
        });

    auto metrics = commonModule()->metrics();
    metrics->tcpConnections().rtsp()++;
    auto metricsGuard = nx::utils::makeScopeGuard(
        [metrics]()
    {
        metrics->tcpConnections().rtsp()--;
    });


    while (!m_needStop && d->socket->isConnected())
    {
        int readed = d->socket->recv(d->tcpReadBuffer, TCP_READ_BUFFER_SIZE);
        if (readed > 0) {
            d->receiveBuffer.append((const char*) d->tcpReadBuffer, readed);
            if (d->receiveBuffer[0] == '$')
            {
                // binary request
                int msgLen = isFullBinaryMessage(d->receiveBuffer);
                if (msgLen)
                {
                    d->clientRequest = d->receiveBuffer.left(msgLen);
                    d->receiveBuffer.remove(0, msgLen);
                    processBinaryRequest();
                }
            }
            else
            {
                // text request
                while (true)
                {
                    const auto msgLen = isFullMessage(d->receiveBuffer);
                    if (msgLen < 0)
                        return;

                    if (msgLen == 0)
                        break;

                    d->clientRequest = d->receiveBuffer.left(msgLen);
                    d->receiveBuffer.remove(0, msgLen);
                    if (!parseRequestParams())
                    {
                        NX_WARNING(this,
                            "RTSP request parsing error: %1, request:\n%2",
                            d->response.messageBody,
                            d->clientRequest);
                        sendResponse(nx::network::http::StatusCode::badRequest, QByteArray());
                        return;
                    }
                    if (!d->peerHasAccess)
                    {
                        sendUnauthorizedResponse(nx::network::http::StatusCode::forbidden);
                        return;
                    }
                    processRequest();
                }
            }
        }
        else if (d->sessionTimeOut > 0)
        {
            if (SystemError::getLastOSErrorCode() == SystemError::timedOut ||
                SystemError::getLastOSErrorCode() == SystemError::again)
            {
                // check rtcp keep alive
                if (d->dataProcessor &&
                    d->dataProcessor->timeFromLastReceiverReport() < kDefaultRtspTimeout)
                    continue;
            }
            break;
        }
    }
}

void QnRtspConnectionProcessor::resetTrackTiming()
{
    Q_D(QnRtspConnectionProcessor);
    for (auto& track: d->trackInfo)
    {
        track->sequence = 0;
        track->firstRtpTime = -1;
        if (auto encoder = track->getEncoder())
            encoder->init();
    }
}

bool QnRtspConnectionProcessor::isTcpMode() const
{
    Q_D(const QnRtspConnectionProcessor);
    return d->tcpMode;
}

QnMediaServerModule* QnRtspConnectionProcessor::serverModule() const
{
    Q_D(const QnRtspConnectionProcessor);
    return d->serverModule;
}
