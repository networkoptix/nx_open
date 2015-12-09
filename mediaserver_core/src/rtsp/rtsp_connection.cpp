
#include <QtCore/QElapsedTimer>
#include <QtCore/QUrlQuery>
#include <utils/common/uuid.h>
#include <QtCore/QSet>
#include <QtCore/QTextStream>
#include <QtCore/QDebug>
#include <QtCore/QBuffer>

#include "libavutil/avutil.h"
#include "libavcodec/avcodec.h"

#include "rtsp_connection.h"
#include "utils/network/rtp_stream_parser.h"
#include "core/dataconsumer/abstract_data_consumer.h"
#include "utils/media/ffmpeg_helper.h"
#include "core/dataprovider/media_streamdataprovider.h"
#include "core/resource_management/resource_pool.h"
#include "core/resource/resource_media_layout.h"
#include "plugins/resource/archive/archive_stream_reader.h"
#include <utils/common/string.h>

#include "utils/network/tcp_connection_priv.h"
#include "utils/network/rtsp/rtsp_types.h"
#include "plugins/resource/archive/abstract_archive_delegate.h"
#include "camera/camera_pool.h"
#include "utils/network/rtpsession.h"
#include "recorder/recording_manager.h"
#include "utils/common/util.h"
#include "rtsp_data_consumer.h"
#include "plugins/resource/server_archive/server_archive_delegate.h"
#include "core/dataprovider/live_stream_provider.h"
#include "core/resource/resource_fwd.h"
#include "core/resource/camera_resource.h"
#include "plugins/resource/server_archive/thumbnails_stream_reader.h"
#include "rtsp/rtsp_encoder.h"
#include "rtsp_h264_encoder.h"
#include "rtsp/rtsp_ffmpeg_encoder.h"
#include "rtp_universal_encoder.h"
#include "utils/common/synctime.h"
#include "utils/network/tcp_listener.h"
#include "network/authenticate_helper.h"
#include <media_server/settings.h>
#include <utils/common/model_functions.h>
#include "http/custom_headers.h"
#include "audit/audit_manager.h"
#include "media_server/settings.h"
#include "streaming/streaming_params.h"
#include "media_server/settings.h"

class QnTcpListener;

static const QByteArray ENDL("\r\n");

//static const int LARGE_RTSP_TIMEOUT = 1000 * 1000 * 50;

// ------------- ServerTrackInfo --------------------

// ----------------------------- QnRtspConnectionProcessorPrivate ----------------------------

enum Mode {Mode_Live, Mode_Archive, Mode_ThumbNails};
//static const int MAX_CAMERA_OPEN_TIME = 1000 * 5;
static const int DEFAULT_RTSP_TIMEOUT = 60; // in seconds
const QString RTSP_CLOCK_FORMAT(QLatin1String("yyyyMMddThhmmssZ"));

QMutex RtspServerTrackInfo::m_createSocketMutex;

bool updatePort(AbstractDatagramSocket* &socket, int port)
{
    delete socket;
    socket = SocketFactory::createDatagramSocket();
    return socket->bind( SocketAddress( HostAddress::anyHost, port ) );
}

bool RtspServerTrackInfo::openServerSocket(const QString& peerAddress)
{
    // try to find a couple of port, even for RTP, odd for RTCP
    QMutexLocker lock(&m_createSocketMutex);
    mediaSocket = SocketFactory::createDatagramSocket();
    rtcpSocket = SocketFactory::createDatagramSocket();

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
        liveMode(Mode_Live),
        auditRecordMode(Mode_Live),
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
        transcodedVideoSize(640, 480)
    {
    }

    QnAbstractMediaStreamDataProviderPtr getCurrentDP()
    {
        if (liveMode == Mode_ThumbNails)
            return thumbnailsDP;
        else if (liveMode == Mode_Live)
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
    Mode liveMode;
    AuditHandle auditRecordHandle;
    Mode auditRecordMode;
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
    QByteArray clientGuid;
    enum CodecID codecId;
    qint64 startTime; // time from last range header
    qint64 endTime;   // time from last range header
    double rtspScale; // RTSP playing speed (1 - normal speed, 0 - pause, >1 fast forward, <-1 fast back e. t.c.)
    QMutex mutex;
    int lastPlayCSeq;
    MediaQuality quality;
    bool qualityFastSwitch;
    int metadataChannelNum;
    bool audioEnabled;
    bool wasDualStreaming;
    bool wasCameraControlDisabled;
    bool tcpMode;
    QSize transcodedVideoSize;
};

// ----------------------------- QnRtspConnectionProcessor ----------------------------

static const CodecID DEFAULT_VIDEO_CODEC = CODEC_ID_H263P; 

QnRtspConnectionProcessor::QnRtspConnectionProcessor(QSharedPointer<AbstractStreamSocket> socket, QnTcpListener* _owner):
    QnTCPConnectionProcessor(new QnRtspConnectionProcessorPrivate, socket)
{
    Q_D(QnRtspConnectionProcessor);
    Q_UNUSED(_owner)
    d->codecId = CODEC_ID_NONE;
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

    QUrl url(d->request.requestLine.url);
    if (d->mediaRes == 0 && d->request.requestLine.url.path() != lit("*"))
    {
        QString resId = url.path();
        if (resId.startsWith('/'))
            resId = resId.mid(1);
        QnResourcePtr resource = qnResPool->getResourceByUrl(resId);
        if (resource == 0) {
            resource = qnResPool->getNetResourceByPhysicalId(resId);
            if (resource == 0) 
                resource = qnResPool->getResourceByMacAddress(resId);
        }
        d->mediaRes = qSharedPointerDynamicCast<QnMediaResource>(resource);
    }

    if (!nx_http::getHeaderValue(d->request.headers, Qn::EC2_INTERNAL_RTP_FORMAT).isNull())
        d->useProprietaryFormat = true;
    else {
        d->sessionTimeOut = DEFAULT_RTSP_TIMEOUT;
        d->socket->setRecvTimeout(d->sessionTimeOut * 1500);
    }

    const QUrlQuery urlQuery( url.query() );

    QString codec = urlQuery.queryItemValue("codec");
    if (!codec.isEmpty())
    {
        AVOutputFormat* format = av_guess_format(codec.toLatin1().data(),NULL,NULL);
        if (format)
            d->codecId = format->video_codec;
        else
            d->codecId = CODEC_ID_NONE;
    };

    const QString pos = urlQuery.queryItemValue( StreamingParams::START_POS_PARAM_NAME ).split('/')[0];
    if (pos.isEmpty())
        processRangeHeader();
    else
        d->startTime = parseDateTime( pos ); //pos.toLongLong();
    QByteArray resolutionStr = urlQuery.queryItemValue("resolution").split('/')[0].toUtf8();
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
        d->transcodedVideoSize = videoSize;
        if (d->codecId == CODEC_ID_NONE)
            d->codecId = DEFAULT_VIDEO_CODEC;
    }

    QString q = nx_http::getHeaderValue(d->request.headers, "x-media-quality");
    if (q == QString("low"))
    {
        d->quality = MEDIA_Quality_Low;
    }
    else if (q == QString("force-high"))
    {
        d->quality = MEDIA_Quality_ForceHigh;
    }
    else if( q.isEmpty() )
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
    else
    {
        d->quality = MEDIA_Quality_High;
    }
    d->qualityFastSwitch = true;
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
    return QHostAddress(d->socket->getForeignAddress().address.ipv4());
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
    d->sessionId = QString::number((long) d->socket.data());
    d->sessionId += QString::number(rand());
}


void QnRtspConnectionProcessor::sendResponse(int code)
{
    QnTCPConnectionProcessor::sendResponse(code, "application/sdp", "", "", true);
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
        d->archiveDP->open();

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
    CodecID dstCodec;
    if (media->dataType == QnAbstractMediaData::VIDEO)
        dstCodec = d->codecId;
    else
        dstCodec = media && media->compressionType == CODEC_ID_AAC ? CODEC_ID_AAC : CODEC_ID_MP2; // keep aac without transcoding for audio
        //dstCodec = media && media->compressionType == CODEC_ID_AAC ? CODEC_ID_AAC : CODEC_ID_VORBIS; // keep aac without transcoding for audio
        //dstCodec = CODEC_ID_AAC; // keep aac without transcoding for audio
    //CodecID dstCodec = media->dataType == QnAbstractMediaData::VIDEO ? CODEC_ID_MPEG4 : media->compressionType;
    QSharedPointer<QnUniversalRtpEncoder> universalEncoder;
    
    QnResourcePtr res = getResource()->toResourcePtr();

    QUrl url(d->request.requestLine.url);
    const QUrlQuery urlQuery( url.query() );
    int rotation;
    if (urlQuery.hasQueryItem("rotation"))
        rotation = urlQuery.queryItemValue("rotation").toInt();
    else
        rotation = res->getProperty(QnMediaResource::rotationKey()).toInt();

    qreal customAR = getResource()->customAspectRatio();

    QnImageFilterHelper extraTranscodeParams;
    extraTranscodeParams.setVideoLayout(vLayout);
    extraTranscodeParams.setRotation(rotation);
    extraTranscodeParams.setCustomAR(customAR);

    switch (dstCodec)
    {
        //case CODEC_ID_H264:
        //    return QnRtspEncoderPtr(new QnRtspH264Encoder());
        case CODEC_ID_NONE:
        case CODEC_ID_H263:
        case CODEC_ID_H263P:
        case CODEC_ID_H264:
        case CODEC_ID_MPEG1VIDEO:
        case CODEC_ID_MPEG2VIDEO:
        case CODEC_ID_MPEG4:
        case CODEC_ID_AAC:
        case CODEC_ID_MP2:
        case CODEC_ID_MP3:
        case CODEC_ID_PCM_ALAW:
        case CODEC_ID_PCM_MULAW:
        case CODEC_ID_PCM_S8:
        case CODEC_ID_PCM_S16BE:
        case CODEC_ID_PCM_S16LE:
        case CODEC_ID_PCM_U16BE:
        case CODEC_ID_PCM_U16LE:
        case CODEC_ID_PCM_U8:
        case CODEC_ID_MPEG2TS:
        case CODEC_ID_AMR_NB:
        case CODEC_ID_AMR_WB:
        case CODEC_ID_VORBIS:
        case CODEC_ID_THEORA:
        case CODEC_ID_VP8:
        case CODEC_ID_ADPCM_G722:
        case CODEC_ID_ADPCM_G726:
            universalEncoder = QSharedPointer<QnUniversalRtpEncoder>(new QnUniversalRtpEncoder(media, dstCodec, resolution, extraTranscodeParams)); // transcode src codec to MPEG4/AAC
            if (MSSettings::roSettings()->value(StreamingParams::FFMPEG_REALTIME_OPTIMIZATION, true).toBool())
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
    QnServerArchiveDelegate archive;
    if (!archive.open(getResource()->toResourcePtr()))
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
        sessionControlUrl.setPort( MSSettings::roSettings()->value( nx_ms_conf::SERVER_PORT, nx_ms_conf::DEFAULT_SERVER_PORT).toInt() );
    sdp << "a=control:" << sessionControlUrl.toString() << ENDL;
#endif
    int i = 0;
    for (; i < numVideo + numAudio; ++i)
    {
        QnRtspEncoderPtr encoder;
        if (d->useProprietaryFormat)
        {
            QnRtspFfmpegEncoder* ffmpegEncoder = new QnRtspFfmpegEncoder();
            encoder = QnRtspEncoderPtr(ffmpegEncoder);
            if (i >= numVideo) 
            {
                QnConstAbstractMediaDataPtr media = getCameraData(i < numVideo ? QnAbstractMediaData::VIDEO : QnAbstractMediaData::AUDIO);
                if (media)
                    ffmpegEncoder->setCodecContext(media->context);
            }

        }
        else {
            QnConstAbstractMediaDataPtr media = getCameraData(i < numVideo ? QnAbstractMediaData::VIDEO : QnAbstractMediaData::AUDIO);
            if (media) 
            {
                encoder = createEncoderByMediaData(media, d->transcodedVideoSize, d->mediaRes->getVideoLayout(d->getCurrentDP().data()));
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
        trackInfo->encoder = encoder;
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

    if (d->liveMode != Mode_ThumbNails && d->useProprietaryFormat)
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
    nx_rtsp::parseRangeHeader(rangeStr, &d->startTime, &d->endTime);
    if (cameraResource && d->startTime == 0 && !d->useProprietaryFormat)
        d->startTime = DATETIME_NOW;
}

void QnRtspConnectionProcessor::at_camera_resourceChanged(const QnResourcePtr & /*resource*/)
{
    Q_D(QnRtspConnectionProcessor);
    QMutexLocker lock(&d->mutex);

    QnVirtualCameraResourcePtr cameraResource = qSharedPointerDynamicCast<QnVirtualCameraResource>(d->mediaRes);
    if (cameraResource) {
        if (cameraResource->isAudioEnabled() != d->audioEnabled ||
            cameraResource->hasDualStreaming2() != d->wasDualStreaming ||
            (!cameraResource->isCameraControlDisabled() && d->wasCameraControlDisabled)) 
        {
            m_needStop = true;
            d->socket->close();
        }
    }
}

void QnRtspConnectionProcessor::at_camera_parentIdChanged(const QnResourcePtr & /*resource*/)
{
    Q_D(QnRtspConnectionProcessor);

    QMutexLocker lock(&d->mutex);
    if (d->mediaRes && d->mediaRes->toResource()->hasFlags(Qn::foreigner)) {
        m_needStop = true;
        d->socket->close();
    }
}

void QnRtspConnectionProcessor::createDataProvider()
{
    Q_D(QnRtspConnectionProcessor);

    const QUrlQuery urlQuery( d->request.requestLine.url.query() );
    QString speedStr = urlQuery.queryItemValue( "speed" );
    if( speedStr.isEmpty() )
        speedStr = nx_http::getHeaderValue( d->request.headers, "Speed" );

    if (!d->dataProcessor) {
        d->dataProcessor = new QnRtspDataConsumer(this);
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
    if (camera && d->liveMode == Mode_Live)
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

            if (cameraRes->hasDualStreaming2())
                d->liveDpLow = camera->getLiveReader(QnServer::LowQualityCatalog);
        }
        if (d->liveDpLow) {
            d->liveDpLow->addDataProcessor(d->dataProcessor);
            d->liveDpLow->startIfNotRunning();
        }
    }
    if (!d->archiveDP) {
        d->archiveDP = QSharedPointer<QnArchiveStreamReader> (dynamic_cast<QnArchiveStreamReader*> (d->mediaRes->toResource()->createDataProvider(Qn::CR_Archive)));
        if (d->archiveDP)
            d->archiveDP->setGroupId(d->clientGuid);
    }

    if (!d->thumbnailsDP && d->liveMode == Mode_ThumbNails) {
        d->thumbnailsDP = QSharedPointer<QnThumbnailsStreamReader>(new QnThumbnailsStreamReader(d->mediaRes->toResourcePtr()));
        d->thumbnailsDP->setGroupId(QnUuid::createUuid().toString().toUtf8());
    }
}

void QnRtspConnectionProcessor::checkQuality()
{
    Q_D(QnRtspConnectionProcessor);
    if (d->liveDpHi && d->quality == MEDIA_Quality_Low)
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

void QnRtspConnectionProcessor::createPredefinedTracks(QnConstResourceVideoLayoutPtr videoLayout)
{
    Q_D(QnRtspConnectionProcessor);

    int trackNum = 0;
    for (; trackNum < videoLayout->channelCount(); ++trackNum)
    {
        RtspServerTrackInfoPtr vTrack(new RtspServerTrackInfo());
        vTrack->encoder = QnRtspEncoderPtr(new QnRtspFfmpegEncoder());
        vTrack->clientPort = trackNum*2;
        vTrack->clientRtcpPort = trackNum*2 + 1;
        d->trackInfo.insert(trackNum, vTrack);
    }

    RtspServerTrackInfoPtr aTrack(new RtspServerTrackInfo());
    aTrack->encoder = QnRtspEncoderPtr(new QnRtspFfmpegEncoder());
    aTrack->clientPort = trackNum*2;
    aTrack->clientRtcpPort = trackNum*2+1;
    d->trackInfo.insert(trackNum, aTrack);

    RtspServerTrackInfoPtr metaTrack(new RtspServerTrackInfo());
    metaTrack->clientPort = d->metadataChannelNum*2;
    metaTrack->clientRtcpPort = d->metadataChannelNum*2+1;
    d->trackInfo.insert(d->metadataChannelNum, metaTrack);

}

int QnRtspConnectionProcessor::composePlay()
{

    Q_D(QnRtspConnectionProcessor);
    if (d->mediaRes == 0)
        return CODE_NOT_FOUND;

    if (!nx_http::getHeaderValue(d->request.headers, "x-media-step").isEmpty())
        d->liveMode = Mode_ThumbNails;
    else if (d->rtspScale >= 0 && d->startTime == DATETIME_NOW)
        d->liveMode = Mode_Live;
    else
        d->liveMode = Mode_Archive;

    createDataProvider();
    checkQuality();

    if (d->trackInfo.isEmpty())
    {
        if (nx_http::getHeaderValue(d->request.headers, "x-play-now").isEmpty())
            return CODE_INTERNAL_ERROR;
        d->clientGuid = nx_http::getHeaderValue(d->request.headers, Qn::GUID_HEADER_NAME);
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

    d->lastPlayCSeq = nx_http::getHeaderValue(d->request.headers, "CSeq").toInt();


    QnVideoCameraPtr camera;
    if (d->mediaRes)
        camera = qnCameraPool->getVideoCamera(d->mediaRes->toResourcePtr());
    if (d->liveMode == Mode_Live) {
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

    d->dataProcessor->setLiveMode(d->liveMode == Mode_Live);

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
    if (d->liveMode == Mode_Live) 
    {
        auto camera = qnCameraPool->getVideoCamera(getResource()->toResourcePtr());
        QMutexLocker dataQueueLock(d->dataProcessor->dataQueueMutex());

        int copySize = 0;
        if (!getResource()->toResource()->hasFlags(Qn::foreigner) && (status == Qn::Online || status == Qn::Recording)) {
            copySize = d->dataProcessor->copyLastGopFromCamera(camera, d->quality != MEDIA_Quality_Low, 0, d->lastPlayCSeq);
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
    else if (d->liveMode == Mode_Archive && d->archiveDP) 
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
    else if (d->liveMode == Mode_ThumbNails && d->thumbnailsDP) 
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
    if (d->liveMode == Mode_Live && d->liveDpLow)
        d->liveDpLow->start();
    
    if (d->liveMode != Mode_ThumbNails) 
    {
        const qint64 startTimeUsec = d->liveMode == Mode_Live ? DATETIME_NOW : d->startTime;
        bool isExport = nx_http::getHeaderValue(d->request.headers, Qn::EC2_MEDIA_ROLE) == "export";
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
            if (vals[1].trimmed() == "low")
                d->quality = MEDIA_Quality_Low;
            else if (vals[1].trimmed() == "force-high")
                d->quality = MEDIA_Quality_ForceHigh;
            else
                d->quality = MEDIA_Quality_High;

            checkQuality();
            d->qualityFastSwitch = false;

            if (d->liveMode == Mode_Live)
            {
                auto camera = qnCameraPool->getVideoCamera(getResource()->toResourcePtr());
                QMutexLocker dataQueueLock(d->dataProcessor->dataQueueMutex());

                d->dataProcessor->setLiveQuality(d->quality);

                qint64 time = d->dataProcessor->lastQueuedTime();
                d->dataProcessor->copyLastGopFromCamera(camera, d->quality != MEDIA_Quality_Low, time, d->lastPlayCSeq); // for fast quality switching

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
    QMutexLocker lock(&d->mutex);

    if (d->dataProcessor)
        d->dataProcessor->pauseNetwork();

    QString method = d->request.requestLine.method;
    if (method != "OPTIONS" && d->sessionId.isEmpty())
        generateSessionId();
    int code = CODE_OK;
    initResponse();
    if (method == "OPTIONS")
    {
        d->response.headers.insert( std::make_pair("Public", "DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE, GET_PARAMETER, SET_PARAMETER") );
    }
    else if (method == "DESCRIBE")
    {
        code = composeDescribe();
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
    }
    else if (method == "SET_PARAMETER")
    {
        code = composeSetParameter();
    }
    sendResponse(code);
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
    bool authOK = false;
    if( MSSettings::roSettings()->value("authenticationEnabled").toBool() )
    {
        for (int i = 0; i < 3 && !m_needStop; ++i)
        {
            if(qnAuthHelper->authenticate(d->request, d->response) != Qn::Auth_OK)
            {
                sendResponse(CODE_AUTH_REQUIRED);
                if (readRequest()) 
                    parseRequest();
                else {
                    authOK = false;
                    break;
                }
            }
            else {
                authOK = true;
                break;
            }
        }
        if (!authOK)
            return;
    }
    else
    {
        authOK = true;
    }

    processRequest();

    QElapsedTimer t;
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
                int msgLen;
                while ((msgLen = isFullMessage(d->receiveBuffer)))
                {
                    d->clientRequest = d->receiveBuffer.left(msgLen);
                    d->receiveBuffer.remove(0, msgLen);
                    parseRequest();
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

    t.restart();

    d->deleteDP();
    d->socket->close();

    d->trackInfo.clear();
    //deleteLater(); // does not works for this thread
}

void QnRtspConnectionProcessor::resetTrackTiming()
{
    Q_D(QnRtspConnectionProcessor);
    for (ServerTrackInfoMap::iterator itr = d->trackInfo.begin(); itr != d->trackInfo.end(); ++itr)
    {
        RtspServerTrackInfoPtr track = itr.value();
        track->sequence = 0;
        track->firstRtpTime = -1;
        if (track->encoder)
            track->encoder->init();
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
    return d->archiveDP;
}
