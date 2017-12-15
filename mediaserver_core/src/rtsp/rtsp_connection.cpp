#include "rtsp_connection.h"

#include <QtCore/QElapsedTimer>
#include <QtCore/QUrlQuery>
#include <nx/utils/uuid.h>
#include <QtCore/QSet>
#include <QtCore/QTextStream>
#include <QtCore/QDebug>
#include <QtCore/QBuffer>

extern "C"
{
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
};

#include <nx/streaming/rtp_stream_parser.h>
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
#include <nx/streaming/rtsp_client.h>
#include <recorder/recording_manager.h>
#include <utils/common/util.h>
#include <rtsp/rtsp_data_consumer.h>
#include <plugins/resource/server_archive/server_archive_delegate.h>
#include <providers/live_stream_provider.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/camera_resource.h>
#include <core/resource/avi/thumbnails_stream_reader.h>
#include <rtsp/rtsp_encoder.h>
#include <rtsp/rtsp_h264_encoder.h>
#include <rtsp/rtsp_ffmpeg_encoder.h>
#include <rtsp/rtp_universal_encoder.h>
#include <utils/common/synctime.h>
#include <network/tcp_listener.h>
#include <network/authenticate_helper.h>
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

class QnTcpListener;

static const QByteArray ENDL("\r\n");

//static const int LARGE_RTSP_TIMEOUT = 1000 * 1000 * 50;

// ------------- ServerTrackInfo --------------------

// ----------------------------- QnRtspConnectionProcessorPrivate ----------------------------

//static const int MAX_CAMERA_OPEN_TIME = 1000 * 5;
static const int DEFAULT_RTSP_TIMEOUT = 60; // in seconds
const QString RTSP_CLOCK_FORMAT(QLatin1String("yyyyMMddThhmmssZ"));

QnMutex RtspServerTrackInfo::m_createSocketMutex;

namespace {

    bool updatePort(AbstractDatagramSocket* &socket, int port)
    {
        delete socket;
        socket = SocketFactory::createDatagramSocket().release();
        return socket->bind(SocketAddress(HostAddress::anyHost, port));
    }

    QByteArray getParamValue(const QByteArray& paramName, const QUrlQuery& urlQuery, const nx_http::HttpHeaders& headers)
    {
        QByteArray paramValue = urlQuery.queryItemValue(paramName).toUtf8();
        if (paramValue.isEmpty())
            paramValue = nx_http::getHeaderValue(headers, QByteArray("x-") + paramName);
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
    mediaSocket = SocketFactory::createDatagramSocket().release();
    rtcpSocket = SocketFactory::createDatagramSocket().release();

    bool opened = mediaSocket->bind( SocketAddress( HostAddress::anyHost, 0 ) );
    if (!opened)
        return false;
    int startPort = mediaSocket->getLocalAddress().port;
    if(startPort&1)
        opened = updatePort(mediaSocket, ++startPort);
    if (opened)
        opened = rtcpSocket->bind( SocketAddress( HostAddress::anyHost, startPort+1 ) );

    while (!opened && startPort < 65534) {
        startPort+=2;
        opened = updatePort(mediaSocket,startPort) && updatePort(rtcpSocket,startPort+1);
    }

    if (opened)
    {
        mediaSocket->setDestAddr(peerAddress, clientPort);
        rtcpSocket->setDestAddr(peerAddress, clientRtcpPort);
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
        startTime(0),
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
        peerHasAccess(true)
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
        delete dataProcessor;
        dataProcessor = 0;

        if (mediaRes) {
            auto camera = qnCameraPool->getVideoCamera(mediaRes->toResourcePtr());
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
    QSharedPointer<QnThumbnailsStreamReader> thumbnailsDP;
    PlaybackMode playbackMode;
    AuditHandle auditRecordHandle;
    QElapsedTimer lastReportTime;
    qint64 lastMediaPacketTime;

    QnRtspDataConsumer* dataProcessor;

    QString sessionId;
    int sessionTimeOut; // timeout in seconds. Not used if zerro
    QnMediaResourcePtr mediaRes;
    //QMap<int, QPair<int,int> > trackPorts; // associate trackID with RTP/RTCP ports (for TCP mode ports used as logical channel numbers, see RFC 2326)
    //QMap<int, QnRtspEncoderPtr> encoders; // associate trackID with RTP codec encoder
    ServerTrackInfoMap trackInfo;
    bool useProprietaryFormat;

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
};

// ----------------------------- QnRtspConnectionProcessor ----------------------------

static const AVCodecID DEFAULT_VIDEO_CODEC = AV_CODEC_ID_H263P;

QnRtspConnectionProcessor::QnRtspConnectionProcessor(QSharedPointer<AbstractStreamSocket> socket, QnTcpListener* owner):
    QnTCPConnectionProcessor(new QnRtspConnectionProcessorPrivate, socket, owner)
{
}

QnRtspConnectionProcessor::~QnRtspConnectionProcessor()
{
    Q_D(QnRtspConnectionProcessor);
    directDisconnectAll();
    if (d->auditRecordHandle && d->lastMediaPacketTime != AV_NOPTS_VALUE)
        qnAuditManager->notifyPlaybackInProgress(d->auditRecordHandle, d->lastMediaPacketTime);

    d->auditRecordHandle.reset();
    stop();
}

void QnRtspConnectionProcessor::notifyMediaRangeUsed(qint64 timestampUsec)
{
    Q_D(QnRtspConnectionProcessor);
    if (d->auditRecordHandle && d->lastReportTime.isValid() && d->lastReportTime.elapsed() >= QnAuditManager::MIN_PLAYBACK_TIME_TO_LOG)
    {
        qnAuditManager->notifyPlaybackInProgress(d->auditRecordHandle, timestampUsec);
        d->lastReportTime.restart();
    }
    d->lastMediaPacketTime = timestampUsec;
}

void QnRtspConnectionProcessor::parseRequest()
{
    Q_D(QnRtspConnectionProcessor);
    QnTCPConnectionProcessor::parseRequest();

    nx_http::HttpHeaders::const_iterator scaleIter = d->request.headers.find("Scale");
    if( scaleIter != d->request.headers.end() )
        d->rtspScale = scaleIter->second.toDouble();

    nx::utils::Url url(d->request.requestLine.url);
    if (d->mediaRes == 0 && d->request.requestLine.url.path() != lit("*"))
    {
        QString resId = url.path();
        if (resId.startsWith('/'))
            resId = resId.mid(1);

        QnResourcePtr resource;
        const QnUuid uuid = QnUuid::fromStringSafe(resId);
        if (!uuid.isNull())
            resource = resourcePool()->getResourceById(uuid);
        if (!resource)
            resource = resourcePool()->getResourceByUniqueId(resId);
        if (!resource)
            resource = resourcePool()->getResourceByMacAddress(resId);
        if (!resource)
            resource = resourcePool()->getResourceByUrl(resId);
        if (!resource)
            return;

        d->mediaRes = qSharedPointerDynamicCast<QnMediaResource>(resource);
    }

    if (!d->mediaRes)
        return;

    if (!nx_http::getHeaderValue(d->request.headers, Qn::EC2_INTERNAL_RTP_FORMAT).isNull())
        d->useProprietaryFormat = true;
    else {
        d->sessionTimeOut = DEFAULT_RTSP_TIMEOUT;
        d->socket->setRecvTimeout(d->sessionTimeOut * 1500);
    }

    const QUrlQuery urlQuery( url.query() );

    d->transcodeParams.codecId = AV_CODEC_ID_NONE;
    QString codec = urlQuery.queryItemValue("codec");
    if (!codec.isEmpty())
    {
        AVOutputFormat* format = av_guess_format(codec.toLatin1().data(),NULL,NULL);
        if (format)
            d->transcodeParams.codecId = format->video_codec;
    };

    const QString pos = urlQuery.queryItemValue( StreamingParams::START_POS_PARAM_NAME ).split('/')[0];
    if (pos.isEmpty())
        processRangeHeader();
    else
        d->startTime = nx::utils::parseDateTime( pos ); //pos.toLongLong();

    d->transcodeParams.resolution = QSize();
    QByteArray resolutionStr = getParamValue("resolution", urlQuery, d->request.headers).split('/')[0];
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
                qWarning() << "Invalid resolution specified for web streaming. Defaulting to 480p";
                videoSize = QSize(0,480);
            }
        }
        d->transcodeParams.resolution = videoSize;
        if (d->transcodeParams.codecId == AV_CODEC_ID_NONE)
            d->transcodeParams.codecId = DEFAULT_VIDEO_CODEC;
    }

    QString qualityStr = nx_http::getHeaderValue(d->request.headers, "x-media-quality");
    if (!qualityStr.isEmpty())
    {
        d->quality = QnLexical::deserialized<MediaQuality>(qualityStr, MEDIA_Quality_High);
    }
    else
    {
        d->quality = MEDIA_Quality_High;

        const QUrlQuery urlQuery( d->request.requestLine.url.query() );
        const QString& streamIndexStr = urlQuery.queryItemValue( "stream" );
        if( !streamIndexStr.isEmpty() )
        {
            const int streamIndex = streamIndexStr.toInt();
            if( streamIndex == 0 )
                d->quality = MEDIA_Quality_High;
            else if( streamIndex == 1 )
                d->quality = MEDIA_Quality_Low;
        }
    }
    d->qualityFastSwitch = true;

    d->peerHasAccess = resourceAccessManager()->hasPermission(d->accessRights,
        d->mediaRes.dynamicCast<QnResource>(),
        requiredPermission(getStreamingMode()));

    d->clientRequest.clear();
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

void QnRtspConnectionProcessor::initResponse(int code, const QString& message)
{
    Q_D(QnRtspConnectionProcessor);

    d->response = nx_http::Response();
    d->response.statusLine.version = d->request.requestLine.version;
    d->response.statusLine.statusCode = code;
    d->response.statusLine.reasonPhrase = message.toUtf8();
    d->response.headers.insert(nx_http::HttpHeader("CSeq", nx_http::getHeaderValue(d->request.headers, "CSeq")));
    QString transport = nx_http::getHeaderValue(d->request.headers, "Transport");
    if (!transport.isEmpty())
        d->response.headers.insert(nx_http::HttpHeader("Transport", transport.toLatin1()));

    if (!d->sessionId.isEmpty()) {
        QString sessionId = d->sessionId;
        if (d->sessionTimeOut > 0)
            sessionId += QString(QLatin1String(";timeout=%1")).arg(d->sessionTimeOut);
        d->response.headers.insert(nx_http::HttpHeader("Session", sessionId.toLatin1()));
    }
}

void QnRtspConnectionProcessor::generateSessionId()
{
    Q_D(QnRtspConnectionProcessor);
    d->sessionId = QString::number(reinterpret_cast<uintptr_t>(d->socket.data()));
    d->sessionId += QString::number(nx::utils::random::number());
}


void QnRtspConnectionProcessor::sendResponse(int httpStatusCode, const QByteArray& contentType)
{
    Q_D(QnTCPConnectionProcessor);

    d->response.statusLine.version = d->request.requestLine.version;
    d->response.statusLine.statusCode = httpStatusCode;
    d->response.statusLine.reasonPhrase =
        nx_http::StatusCode::toString((nx_http::StatusCode::Value)httpStatusCode);

    nx_http::insertOrReplaceHeader(
        &d->response.headers,
        nx_http::HttpHeader(nx_http::header::Server::NAME, nx_http::serverString()));
    nx_http::insertOrReplaceHeader(
        &d->response.headers,
        nx_http::HttpHeader("Date", nx_http::formatDateTime(QDateTime::currentDateTime())));

    if (!contentType.isEmpty())
        nx_http::insertOrReplaceHeader(
            &d->response.headers,
            nx_http::HttpHeader("Content-Type", contentType));
    if (!d->response.messageBody.isEmpty())
        nx_http::insertOrReplaceHeader(
            &d->response.headers,
            nx_http::HttpHeader(
                "Content-Length",
                QByteArray::number(d->response.messageBody.size())));

    const QByteArray response = d->response.toString();

    NX_LOG(lit("Server response to %1:\n%2").
        arg(d->socket->getForeignAddress().address.toString()).
        arg(QString::fromLatin1(response)),
        cl_logDEBUG1);

    NX_LOG(QnLog::HTTP_LOG_INDEX,
        lit("Sending response to %1:\n%2\n-------------------\n\n\n").
        arg(d->socket->getForeignAddress().toString()).
        arg(QString::fromLatin1(response)), cl_logDEBUG1);

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

/*
QnRtspEncoderPtr QnRtspConnectionProcessor::getCodecEncoder(int trackNum) const
{
    Q_D(const QnRtspConnectionProcessor);
    ServerTrackInfoMap::const_iterator itr = d->trackInfo.find(trackNum);
    if (itr != d->trackInfo.end())
        return itr.value()->encoder;
    else
        return QnRtspEncoderPtr();
}

UDPSocket* QnRtspConnectionProcessor::getMediaSocket(int trackNum) const
{
    Q_D(const QnRtspConnectionProcessor);
    if (d->tcpMode)
        return 0;

    ServerTrackInfoMap::const_iterator itr = d->trackInfo.find(trackNum);
    if (itr != d->trackInfo.end())
        return &(itr.value()->mediaSocket);
    else
        return 0;
}
*/

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
        qint64 archiveEndTime = d->archiveDP->endTime();
        bool endTimeIsNow = QnRecordingManager::instance()->isCameraRecoring(d->archiveDP->getResource()); // && !endTimeInFuture;
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
            if (QnRecordingManager::instance()->isCameraRecoring(d->archiveDP->getResource()))
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

    if (d->archiveDP && !d->archiveDP->offlineRangeSupported())
        d->archiveDP->open(qnServerModule->archiveIntegrityWatcher());

    QByteArray range = getRangeStr();
    if (!range.isEmpty())
    {
        nx_http::insertOrReplaceHeader(
            &d->response.headers,
            nx_http::HttpHeader( "Range", range ) );
    }
};

QnRtspEncoderPtr QnRtspConnectionProcessor::createEncoderByMediaData(QnConstAbstractMediaDataPtr media, QSize resolution, QnConstResourceVideoLayoutPtr vLayout)
{
    Q_D(QnRtspConnectionProcessor);
    AVCodecID dstCodec;
    if (media->dataType == QnAbstractMediaData::VIDEO)
        dstCodec = d->transcodeParams.codecId;
    else
        dstCodec = media && media->compressionType == AV_CODEC_ID_AAC ? AV_CODEC_ID_AAC : AV_CODEC_ID_MP2; // keep aac without transcoding for audio
        //dstCodec = media && media->compressionType == AV_CODEC_ID_AAC ? AV_CODEC_ID_AAC : AV_CODEC_ID_VORBIS; // keep aac without transcoding for audio
        //dstCodec = AV_CODEC_ID_AAC; // keep aac without transcoding for audio
    //AVCodecID dstCodec = media->dataType == QnAbstractMediaData::VIDEO ? AV_CODEC_ID_MPEG4 : media->compressionType;
    QSharedPointer<QnUniversalRtpEncoder> universalEncoder;

    QnResourcePtr res = getResource()->toResourcePtr();

    nx::utils::Url url(d->request.requestLine.url);
    const QUrlQuery urlQuery( url.query() );
    int rotation;
    if (urlQuery.hasQueryItem("rotation"))
        rotation = urlQuery.queryItemValue("rotation").toInt();
    else
        rotation = res->getProperty(QnMediaResource::rotationKey()).toInt();

    qreal customAR = getResource()->customAspectRatio();

    QnLegacyTranscodingSettings extraTranscodeParams;
    extraTranscodeParams.resource = getResource();
    extraTranscodeParams.rotation = rotation;
    extraTranscodeParams.forcedAspectRatio = customAR;

    switch (dstCodec)
    {
        //case AV_CODEC_ID_H264:
        //    return QnRtspEncoderPtr(new QnRtspH264Encoder());
        case AV_CODEC_ID_NONE:
        case AV_CODEC_ID_H263:
        case AV_CODEC_ID_H263P:
        case AV_CODEC_ID_H264:
        case AV_CODEC_ID_MPEG1VIDEO:
        case AV_CODEC_ID_MPEG2VIDEO:
        case AV_CODEC_ID_MPEG4:
        case AV_CODEC_ID_AAC:
        case AV_CODEC_ID_MP2:
        case AV_CODEC_ID_MP3:
        case AV_CODEC_ID_PCM_ALAW:
        case AV_CODEC_ID_PCM_MULAW:
        case AV_CODEC_ID_PCM_S8:
        case AV_CODEC_ID_PCM_S16BE:
        case AV_CODEC_ID_PCM_S16LE:
        case AV_CODEC_ID_PCM_U16BE:
        case AV_CODEC_ID_PCM_U16LE:
        case AV_CODEC_ID_PCM_U8:
        case AV_CODEC_ID_MPEG2TS:
        case AV_CODEC_ID_AMR_NB:
        case AV_CODEC_ID_AMR_WB:
        case AV_CODEC_ID_VORBIS:
        case AV_CODEC_ID_THEORA:
        case AV_CODEC_ID_VP8:
        case AV_CODEC_ID_ADPCM_G722:
        case AV_CODEC_ID_ADPCM_G726:
            universalEncoder = QSharedPointer<QnUniversalRtpEncoder>(new QnUniversalRtpEncoder(
                commonModule(),
                media,
                dstCodec,
                resolution,
                extraTranscodeParams)); // transcode src codec to MPEG4/AAC
            if (qnServerModule->roSettings()->value(StreamingParams::FFMPEG_REALTIME_OPTIMIZATION, true).toBool())
                universalEncoder->setUseRealTimeOptimization(true);
            if (universalEncoder->isOpened())
                return universalEncoder;
            else
                return QnRtspEncoderPtr();
        default:
            return QnRtspEncoderPtr();
    }
    return QnRtspEncoderPtr();
}

QnConstAbstractMediaDataPtr QnRtspConnectionProcessor::getCameraData(QnAbstractMediaData::DataType dataType)
{
    Q_D(QnRtspConnectionProcessor);

    QnConstAbstractMediaDataPtr rez;

    bool isHQ = d->quality == MEDIA_Quality_High || d->quality == MEDIA_Quality_ForceHigh;

    // 1. check packet in GOP keeper
    // Do not check audio for live point if not proprietary client
    bool canCheckLive = (dataType == QnAbstractMediaData::VIDEO) || (d->startTime == DATETIME_NOW);
    if (canCheckLive)
    {
        QnVideoCameraPtr camera;
        if (getResource())
            camera = qnCameraPool->getVideoCamera(getResource()->toResourcePtr());
        if (camera) {
            if (dataType == QnAbstractMediaData::VIDEO)
                rez =  camera->getLastVideoFrame(isHQ, 0);
            else
                rez = camera->getLastAudioFrame(isHQ);
            if (rez)
                return rez;
        }
    }

    // 2. find packet inside archive
    QnServerArchiveDelegate archive(qnServerModule);
    if (!archive.open(getResource()->toResourcePtr(), qnServerModule->archiveIntegrityWatcher()))
        return rez;
    if (d->startTime != DATETIME_NOW)
        archive.seek(d->startTime, true);
    if (archive.getAudioLayout()->channelCount() == 0 && dataType == QnAbstractMediaData::AUDIO)
        return rez;

    for (int i = 0; i < 40; ++i)
    {
        QnConstAbstractMediaDataPtr media = archive.getNextData();
        if (!media)
            return rez;
        if (media->dataType == dataType)
            return media;
    }
    return rez;
}

QnConstMediaContextPtr QnRtspConnectionProcessor::getAudioCodecContext(int audioTrackIndex) const
{
    Q_D(const QnRtspConnectionProcessor);

    QnServerArchiveDelegate archive(qnServerModule);
    QnConstResourceAudioLayoutPtr layout;

    if (d->startTime == DATETIME_NOW)
    {
        layout = d->mediaRes->getAudioLayout(d->liveDpHi.data()); //< Layout from live video.
    }
    else if (archive.open(getResource()->toResourcePtr(), qnServerModule->archiveIntegrityWatcher()))
    {
        archive.seek(d->startTime, /*findIFrame*/ true);
        layout = archive.getAudioLayout(); //< Current position in archive.
    }

    if (layout && audioTrackIndex < layout->channelCount())
        return layout->getAudioTrackInfo(audioTrackIndex).codecContext;
    return QnConstMediaContextPtr(); //< Not found.
}

int QnRtspConnectionProcessor::composeDescribe()
{
    Q_D(QnRtspConnectionProcessor);
    if (!d->mediaRes)
        return CODE_NOT_FOUND;

    createDataProvider();

    QString acceptMethods = nx_http::getHeaderValue(d->request.headers, "Accept");
    if (acceptMethods.indexOf("sdp") == -1)
        return CODE_NOT_IMPLEMETED;

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

    int numVideo = videoLayout && d->useProprietaryFormat ? videoLayout->channelCount() : 1;

    addResponseRangeHeader();


    sdp << "v=0" << ENDL;
    sdp << "s=" << d->mediaRes->toResource()->getName() << ENDL;
    sdp << "c=IN IP4 " << d->socket->getLocalAddress().address.toString() << ENDL;
#if 0
    QUrl sessionControlUrl = d->request.requestLine.url;
    if( sessionControlUrl.port() == -1 )
        sessionControlUrl.setPort( qnServerModule->roSettings()->value( nx_ms_conf::SERVER_PORT, nx_ms_conf::DEFAULT_SERVER_PORT).toInt() );
    sdp << "a=control:" << sessionControlUrl.toString() << ENDL;
#endif
    int i = 0;
    for (; i < numVideo + numAudio; ++i)
    {
        QnRtspEncoderPtr encoder;
        if (d->useProprietaryFormat)
        {
            bool isVideoTrack = (i < numVideo);
            QnRtspFfmpegEncoder* ffmpegEncoder = createRtspFfmpegEncoder(isVideoTrack);
            if (!isVideoTrack)
            {
                const int audioTrackIndex = i - numVideo;
                ffmpegEncoder->setCodecContext(getAudioCodecContext(audioTrackIndex));
            }
            encoder = QnRtspEncoderPtr(ffmpegEncoder);
        }
        else {
            QnConstAbstractMediaDataPtr media = getCameraData(i < numVideo ? QnAbstractMediaData::VIDEO : QnAbstractMediaData::AUDIO);
            if (media)
            {
                encoder = createEncoderByMediaData(media, d->transcodeParams.resolution, d->mediaRes->getVideoLayout(d->getCurrentDP().data()));
                if (encoder)
                    encoder->setMediaData(media);
                else
                {
                    qWarning() << "no RTSP encoder for codec " << media->compressionType << "skip track";
                    if (i >= numVideo)
                        continue; // if audio is not found do not report error. just skip track
                }
            }
            else if (i >= numVideo) {
                continue; // if audio is not found do not report error. just skip track
            }
        }
        if (encoder == 0)
            return CODE_NOT_FOUND;

        RtspServerTrackInfoPtr trackInfo(new RtspServerTrackInfo());
        trackInfo->setEncoder(encoder);
        d->trackInfo.insert(i, trackInfo);
        //d->encoders.insert(i, encoder);

        const CameraMediaStreams supportedMediaStreams = QJson::deserialized<CameraMediaStreams>(
            d->mediaRes->toResource()->getProperty( Qn::CAMERA_MEDIA_STREAM_LIST_PARAM_NAME ).toLatin1() );

        const QUrlQuery urlQuery( d->request.requestLine.url.query() );
        const QString& streamIndexStr = urlQuery.queryItemValue( "stream" );
        const int mediaStreamIndex = streamIndexStr.isEmpty() ? 0 : streamIndexStr.toInt();

        auto mediaStreamIter = std::find_if(
            supportedMediaStreams.streams.cbegin(),
            supportedMediaStreams.streams.cend(),
            [mediaStreamIndex]( const CameraMediaStreamInfo& streamInfo ) {
                return streamInfo.encoderIndex == mediaStreamIndex;
            } );
        const std::map<QString, QString>& streamParams =
            mediaStreamIter != supportedMediaStreams.streams.cend()
            ? mediaStreamIter->customStreamParams
            : std::map<QString, QString>();

        //sdp << "m=" << (i < numVideo ? "video " : "audio ") << i << " RTP/AVP " << encoder->getPayloadtype() << ENDL;
        sdp << "m=" << (i < numVideo ? "video " : "audio ") << 0 << " RTP/AVP " << encoder->getPayloadtype() << ENDL;
#if 1
        sdp << "a=control:trackID=" << i << ENDL;
#else
        QUrl subSessionControlUrl = sessionControlUrl;
        subSessionControlUrl.setPath( subSessionControlUrl.path() + lit("/trackID=%1").arg(i) );
        sdp << "a=control:" << subSessionControlUrl.toString()<< ENDL;
#endif
        QByteArray additionSDP = encoder->getAdditionSDP( streamParams );
        if (!additionSDP.contains("a=rtpmap:"))
            sdp << "a=rtpmap:" << encoder->getPayloadtype() << ' ' << encoder->getName() << "/" << encoder->getFrequency() << ENDL;
        sdp << additionSDP;
    }

    if (d->playbackMode != PlaybackMode::ThumbNails && d->useProprietaryFormat)
    {
        RtspServerTrackInfoPtr trackInfo(new RtspServerTrackInfo());
        d->trackInfo.insert(d->metadataChannelNum, trackInfo);

        sdp << "m=metadata " << d->metadataChannelNum << " RTP/AVP " << RTP_METADATA_CODE << ENDL;
        sdp << "a=control:trackID=" << d->metadataChannelNum << ENDL;
        sdp << "a=rtpmap:" << RTP_METADATA_CODE << ' ' << RTP_METADATA_GENERIC_STR << "/" << CLOCK_FREQUENCY << ENDL;
    }
    return CODE_OK;
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

int QnRtspConnectionProcessor::composeSetup()
{
    Q_D(QnRtspConnectionProcessor);
    if (!d->mediaRes)
        return CODE_NOT_FOUND;

    QByteArray transport = nx_http::getHeaderValue(d->request.headers, "Transport");
    //if (transport.indexOf("TCP") == -1)
    //    return CODE_NOT_IMPLEMETED;
    //QByteArray lowLevelTransport = transport.split(';').first().split('/').last().toLower();
    //if (lowLevelTransport != "tcp" && lowLevelTransport != "udp")
    //    return CODE_NOT_IMPLEMETED;
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
                    //if (trackInfo->getSSRC())
                    //    transport.append(";ssrc=").append(QByteArray::number(trackInfo->getSSRC()));
                }
            }
        }
    }
    d->response.headers.insert(nx_http::HttpHeader("Transport", transport));
    return CODE_OK;
}

int QnRtspConnectionProcessor::composePause()
{
    //Q_D(QnRtspConnectionProcessor);
    //if (!d->dataProvider)
    //    return CODE_NOT_FOUND;

    //if (d->archiveDP)
    //    d->archiveDP->setSingleShotMode(true);

    //d->playTime += d->rtspScale * d->playTimer.elapsed()*1000;
    //d->rtspScale = 0;

    //d->state = QnRtspConnectionProcessorPrivate::State_Paused;
    return CODE_OK;
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
    const auto rangeStr = nx_http::getHeaderValue(d->request.headers, "Range");
    QnVirtualCameraResourcePtr cameraResource = qSharedPointerDynamicCast<QnVirtualCameraResource>(d->mediaRes);
    if (!nx_rtsp::parseRangeHeader(rangeStr, &d->startTime, &d->endTime))
        d->startTime = DATETIME_NOW;
}

void QnRtspConnectionProcessor::at_camera_resourceChanged(const QnResourcePtr & /*resource*/)
{
    Q_D(QnRtspConnectionProcessor);
    QnMutexLocker lock( &d->mutex );

    QnVirtualCameraResourcePtr cameraResource = qSharedPointerDynamicCast<QnVirtualCameraResource>(d->mediaRes);
    if (cameraResource) {
        if (cameraResource->isAudioEnabled() != d->audioEnabled ||
            cameraResource->hasDualStreaming2() != d->wasDualStreaming ||
            (!cameraResource->isCameraControlDisabled() && d->wasCameraControlDisabled))
        {
            m_needStop = true;
            if (auto socket = d->socket)
                socket->shutdown();
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
        if (auto socket = d->socket)
            socket->shutdown();
    }
}

void QnRtspConnectionProcessor::createDataProvider()
{
    Q_D(QnRtspConnectionProcessor);

    const QUrlQuery urlQuery( d->request.requestLine.url.query() );
    QString speedStr = urlQuery.queryItemValue( "speed" );
    if( speedStr.isEmpty() )
        speedStr = nx_http::getHeaderValue( d->request.headers, "Speed" );

    const auto clientGuid = nx_http::getHeaderValue(d->request.headers, Qn::GUID_HEADER_NAME);

    if (!d->dataProcessor) {
        d->dataProcessor = new QnRtspDataConsumer(this);
        if (d->mediaRes)
            d->dataProcessor->setResource(d->mediaRes->toResourcePtr());
        d->dataProcessor->pauseNetwork();
        int speed = 1;  //real time
        if( d->useProprietaryFormat )
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
        d->dataProcessor->setMultiChannelVideo(d->useProprietaryFormat);
    }
    else
        d->dataProcessor->clearUnprocessedData();

    QnVideoCameraPtr camera;
    if (d->mediaRes) {
        camera = qnCameraPool->getVideoCamera(d->mediaRes->toResourcePtr());
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
            if (cameraRes->hasDualStreaming2() && cameraRes->isEnoughFpsToRunSecondStream(fps))
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
        d->archiveDP = QSharedPointer<QnArchiveStreamReader> (dynamic_cast<QnArchiveStreamReader*> (d->mediaRes->toResource()->createDataProvider(Qn::CR_Archive)));
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
            archiveDelegate = new QnServerArchiveDelegate(qnServerModule); // default value
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
            qWarning() << "Low quality not supported for camera" << d->mediaRes->toResource()->getUniqueId();
        }
        else if (d->liveDpLow->isPaused()) {
            d->quality = MEDIA_Quality_High;
            qWarning() << "Primary stream has big fps for camera" << d->mediaRes->toResource()->getUniqueId() << ". Secondary stream is disabled.";
        }
    }
    QnVirtualCameraResourcePtr cameraRes = d->mediaRes.dynamicCast<QnVirtualCameraResource>();
    if (cameraRes) {
        d->wasDualStreaming = cameraRes->hasDualStreaming2();
        d->wasCameraControlDisabled = cameraRes->isCameraControlDisabled();
    }
}

QnRtspFfmpegEncoder* QnRtspConnectionProcessor::createRtspFfmpegEncoder(bool isVideo)
{
    Q_D(const QnRtspConnectionProcessor);

    QnRtspFfmpegEncoder* result = new QnRtspFfmpegEncoder();
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
                track->setEncoder(QnRtspEncoderPtr(createRtspFfmpegEncoder(true)));
        }
    }
}

void QnRtspConnectionProcessor::createPredefinedTracks(QnConstResourceVideoLayoutPtr videoLayout)
{
    Q_D(QnRtspConnectionProcessor);

    int trackNum = 0;
    for (; trackNum < videoLayout->channelCount(); ++trackNum)
    {
        RtspServerTrackInfoPtr vTrack(new RtspServerTrackInfo());
        vTrack->setEncoder(QnRtspEncoderPtr(createRtspFfmpegEncoder(true)));
        vTrack->clientPort = trackNum*2;
        vTrack->clientRtcpPort = trackNum*2 + 1;
        vTrack->mediaType = RtspServerTrackInfo::MediaType::Video;
        d->trackInfo.insert(trackNum, vTrack);
    }

    RtspServerTrackInfoPtr aTrack(new RtspServerTrackInfo());
    aTrack->setEncoder(QnRtspEncoderPtr(new QnRtspFfmpegEncoder()));
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

    if (!nx_http::getHeaderValue(d->request.headers, "x-media-step").isEmpty())
        return PlaybackMode::ThumbNails;
    else if (d->rtspScale >= 0 && d->startTime == DATETIME_NOW)
        return PlaybackMode::Live;
    const bool isExport = nx_http::getHeaderValue(d->request.headers, Qn::EC2_MEDIA_ROLE) == "export";
    return isExport ? PlaybackMode::Export : PlaybackMode::Archive;
}

int QnRtspConnectionProcessor::composePlay()
{

    Q_D(QnRtspConnectionProcessor);
    if (d->mediaRes == 0)
        return CODE_NOT_FOUND;

    d->playbackMode = getStreamingMode();

    createDataProvider();
    checkQuality();

    if (d->trackInfo.isEmpty())
    {
        if (nx_http::getHeaderValue(d->request.headers, "x-play-now").isEmpty())
            return CODE_INTERNAL_ERROR;
        d->useProprietaryFormat = true;
        d->sessionTimeOut = 0;
        //d->socket->setRecvTimeout(LARGE_RTSP_TIMEOUT);
        //d->socket->setSendTimeout(LARGE_RTSP_TIMEOUT); // set large timeout for native connection
        QnConstResourceVideoLayoutPtr videoLayout = d->mediaRes->getVideoLayout(d->liveDpHi.data());
        createPredefinedTracks(videoLayout);
        if (videoLayout) {
            QString layoutStr = videoLayout->toString();
            if (!layoutStr.isEmpty())
                d->response.headers.insert( std::make_pair(nx_http::StringType("x-video-layout"), layoutStr.toLatin1()) );
        }
    }
    else if (d->useProprietaryFormat)
    {
        updatePredefinedTracks();
    }

    d->prevTranscodeParams = d->transcodeParams;
    d->lastPlayCSeq = nx_http::getHeaderValue(d->request.headers, "CSeq").toInt();


    QnVideoCameraPtr camera;
    if (d->mediaRes)
        camera = qnCameraPool->getVideoCamera(d->mediaRes->toResourcePtr());
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
        return CODE_NOT_FOUND;


    Qn::ResourceStatus status = getResource()->toResource()->getStatus();

    d->dataProcessor->setLiveMode(d->playbackMode == PlaybackMode::Live);

    if (!d->useProprietaryFormat)
    {
        d->quality = MEDIA_Quality_High;

        const QUrlQuery urlQuery( d->request.requestLine.url.query() );
        const QString& streamIndexStr = urlQuery.queryItemValue( "stream" );
        if( !streamIndexStr.isEmpty() )
        {
            const int streamIndex = streamIndexStr.toInt();
            if( streamIndex == 0 )
                d->quality = MEDIA_Quality_High;
            else if( streamIndex == 1 )
                d->quality = MEDIA_Quality_Low;
        }
    }

    //QnArchiveStreamReader* archiveProvider = dynamic_cast<QnArchiveStreamReader*> (d->dataProvider);

    if (d->playbackMode == PlaybackMode::Live)
    {
        auto camera = qnCameraPool->getVideoCamera(getResource()->toResourcePtr());
        if (!camera)
            return CODE_NOT_FOUND;

        QnMutexLocker dataQueueLock(d->dataProcessor->dataQueueMutex());
        int copySize = 0;
        if (!getResource()->toResource()->hasFlags(Qn::foreigner) && (status == Qn::Online || status == Qn::Recording))
        {
            bool usePrimaryStream =
                d->quality != MEDIA_Quality_Low && d->quality != MEDIA_Quality_LowIframesOnly;
            bool iFramesOnly = d->quality == MEDIA_Quality_LowIframesOnly;
            copySize = d->dataProcessor->copyLastGopFromCamera(
                camera,
                usePrimaryStream,
                0, /* skipTime */
                d->lastPlayCSeq,
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
        quint32 cseq = copySize > 0 ? d->lastPlayCSeq : 0;
        d->dataProcessor->setWaitCSeq(d->startTime, cseq); // ignore rest packets before new position
        d->dataProcessor->setLiveQuality(d->quality);
        d->dataProcessor->setLiveMarker(d->lastPlayCSeq);
    }
    else if ((d->playbackMode == PlaybackMode::Archive || d->playbackMode == PlaybackMode::Export)
              && d->archiveDP)
    {
        d->archiveDP->addDataProcessor(d->dataProcessor);

        QString sendMotion = nx_http::getHeaderValue(d->request.headers, "x-send-motion");

        d->archiveDP->lock();

        if (!sendMotion.isNull())
            d->archiveDP->setSendMotion(sendMotion == "1" || sendMotion == "true");

        d->archiveDP->setSpeed(d->rtspScale);
        d->archiveDP->setQuality(d->quality, d->qualityFastSwitch);
        if (d->startTime > 0)
        {
            d->dataProcessor->setSingleShotMode(d->startTime != DATETIME_NOW && d->startTime == d->endTime);
            d->dataProcessor->setWaitCSeq(d->startTime, d->lastPlayCSeq); // ignore rest packets before new position
            bool findIFrame = nx_http::getHeaderValue( d->request.headers, "x-no-find-iframe" ).isNull();
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
        d->thumbnailsDP->setRange(d->startTime, d->endTime, nx_http::getHeaderValue(d->request.headers, "x-media-step").toLongLong(), d->lastPlayCSeq);
        d->thumbnailsDP->setQuality(d->quality);
    }

    d->dataProcessor->setUseUTCTime(d->useProprietaryFormat);
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
        d->auditRecordHandle = qnAuditManager->notifyPlaybackStarted(authSession(), d->mediaRes->toResource()->getId(), startTimeUsec, isExport);
        d->lastReportTime.restart();
    }

    return CODE_OK;
}

int QnRtspConnectionProcessor::composeTeardown()
{
    Q_D(QnRtspConnectionProcessor);

    d->deleteDP();
    d->mediaRes = QnMediaResourcePtr(0);

    d->rtspScale = 1.0;
    d->startTime = d->endTime = 0;

    //d->encoders.clear();

    return CODE_OK;
}

int QnRtspConnectionProcessor::composeSetParameter()
{
    Q_D(QnRtspConnectionProcessor);

    if (!d->mediaRes)
        return CODE_NOT_FOUND;

    createDataProvider();

    QList<QByteArray> parameters = d->requestBody.split('\n');
    for(const QByteArray& parameter: parameters)
    {
        QByteArray normParam = parameter.trimmed().toLower();
        QList<QByteArray> vals = parameter.split(':');
        if (vals.size() < 2)
            return CODE_INVALID_PARAMETER;
        if (normParam.startsWith("x-media-quality"))
        {
            QString q = vals[1].trimmed();
            d->quality = QnLexical::deserialized<MediaQuality>(q, MEDIA_Quality_High);

            checkQuality();
            d->qualityFastSwitch = false;

            if (d->playbackMode == PlaybackMode::Live)
            {
                auto camera = qnCameraPool->getVideoCamera(getResource()->toResourcePtr());
                QnMutexLocker dataQueueLock(d->dataProcessor->dataQueueMutex());

                d->dataProcessor->setLiveQuality(d->quality);

                qint64 time = d->dataProcessor->lastQueuedTime();
                bool usePrimaryStream = d->quality != MEDIA_Quality_Low && d->quality != MEDIA_Quality_LowIframesOnly;
                bool iFramesOnly = d->quality == MEDIA_Quality_LowIframesOnly;
                d->dataProcessor->copyLastGopFromCamera(
                    camera,
                    usePrimaryStream,
                    time,
                    d->lastPlayCSeq,
                    iFramesOnly); // for fast quality switching

                // set "main" dataProvider. RTSP data consumer is going to unsubscribe from other dataProvider
                // then it will be possible (I frame received)
            }
            d->archiveDP->setQuality(d->quality, d->qualityFastSwitch);
            return CODE_OK;
        }
        else if (normParam.startsWith("x-send-motion") && d->archiveDP)
        {
            QByteArray value = vals[1].trimmed();
            d->archiveDP->setSendMotion(value == "1" || value == "true");
            return CODE_OK;
        }
    }

    return CODE_INVALID_PARAMETER;
}

int QnRtspConnectionProcessor::composeGetParameter()
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
            qWarning() << Q_FUNC_INFO << __LINE__ << "Unsupported RTSP parameter " << parameter.trimmed();
            return CODE_INVALID_PARAMETER;
        }
    }

    return CODE_OK;
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
    int code = CODE_OK;
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
    d->socket->setSendBufferSize(16*1024);
    //d->socket->setRecvTimeout(1000*1000);
    //d->socket->setSendTimeout(1000*1000);

    if (d->clientRequest.isEmpty())
        readRequest();

    parseRequest();

    if (!d->peerHasAccess)
    {
        sendUnauthorizedResponse(nx_http::StatusCode::forbidden, STATIC_FORBIDDEN_HTML);
        return;
    }

    processRequest();
    QElapsedTimer t;

    auto guard = makeScopeGuard(
        [&t, d]()
        {
            t.restart();
            d->deleteDP();
            d->socket->close();
            d->trackInfo.clear();
        });

    while (!m_needStop && d->socket->isConnected())
    {
        t.restart();
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
            else {
                // text request
                while (true)
                {
                    const auto msgLen = isFullMessage(d->clientRequest);
                    if (msgLen < 0)
                        return;

                    if (msgLen == 0)
                        break;

                    d->clientRequest = d->receiveBuffer.left(msgLen);
                    d->receiveBuffer.remove(0, msgLen);
                    parseRequest();
                    if (!d->peerHasAccess)
                    {
                        sendUnauthorizedResponse(nx_http::StatusCode::forbidden, STATIC_FORBIDDEN_HTML);
                        return;
                    }
                    processRequest();
                }
            }
        }
        else if (t.elapsed() < 2)
        {
            // recv return 0 bytes imediatly, so client has closed socket
            break;
        }
        else if (d->sessionTimeOut > 0)
        {
            // read timeout
            break;
        }
    }
}

void QnRtspConnectionProcessor::resetTrackTiming()
{
    Q_D(QnRtspConnectionProcessor);
    for (ServerTrackInfoMap::const_iterator itr = d->trackInfo.constBegin(); itr != d->trackInfo.constEnd(); ++itr)
    {
        RtspServerTrackInfoPtr track = itr.value();
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

QSharedPointer<QnArchiveStreamReader> QnRtspConnectionProcessor::getArchiveDP()
{
    Q_D(QnRtspConnectionProcessor);
    QnMutexLocker lock(&d->archiveDpMutex);
    return d->archiveDP;
}
