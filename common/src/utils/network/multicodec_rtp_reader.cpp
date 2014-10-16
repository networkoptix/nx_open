#include "multicodec_rtp_reader.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <QtCore/QSettings>

#include <business/events/reasoned_business_event.h>
#include <business/events/network_issue_business_event.h>

#include "utils/common/log.h"
#include "utils/common/synctime.h"
#include "utils/common/util.h"
#include "utils/network/rtp_stream_parser.h"
#include "utils/network/compat_poll.h"
#include "utils/network/h264_rtp_parser.h"

#include "core/resource/network_resource.h"
#include "core/resource/resource_media_layout.h"
#include "core/resource/media_resource.h"
#include "core/resource/camera_resource.h"

#include "aac_rtp_parser.h"
#include "simpleaudio_rtp_parser.h"
#include "mjpeg_rtp_parser.h"
#include "api/app_server_connection.h"


static const int RTSP_RETRY_COUNT = 6;
static const int RTCP_REPORT_TIMEOUT = 30 * 1000;

static RtpTransport::Value defaultTransportToUse( RtpTransport::_auto );

QnMulticodecRtpReader::QnMulticodecRtpReader(const QnResourcePtr& res):
    QnResourceConsumer(res),
    m_timeHelper(res->getUniqueId()),
    m_pleaseStop(false),
    m_gotSomeFrame(false),
    m_role(Qn::CR_Default),
    m_gotData(false)
{
    QnNetworkResourcePtr netRes = qSharedPointerDynamicCast<QnNetworkResource>(res);
    if (netRes)
        m_RtpSession.setTCPTimeout(netRes->getNetworkTimeout());
    else
        m_RtpSession.setTCPTimeout(1000 * 10);
    QnMediaResourcePtr mr = qSharedPointerDynamicCast<QnMediaResource>(res);
    m_numberOfVideoChannels = 1;
    m_customVideoLayout.clear();

    QnSecurityCamResourcePtr camRes = qSharedPointerDynamicCast<QnSecurityCamResource>(res);
    if (camRes)
        connect(this, &QnMulticodecRtpReader::networkIssue, camRes.data(), &QnSecurityCamResource::networkIssue);
    connect(res.data(), SIGNAL(propertyChanged(const QnResourcePtr &, const QString &)),          this, SLOT(at_propertyChanged(const QnResourcePtr &, const QString &)));
}

QnMulticodecRtpReader::~QnMulticodecRtpReader()
{
    for (unsigned int i = 0; i < m_demuxedData.size(); ++i)
        delete m_demuxedData[i];
}

void QnMulticodecRtpReader::setRequest(const QString& request)
{
    m_request = request;
}

void QnMulticodecRtpReader::clearKeyData(int channelNum)
{
    if (m_gotKeyDataInfo.size() > channelNum)
        m_gotKeyDataInfo[channelNum] = false;
}

bool QnMulticodecRtpReader::gotKeyData(const QnAbstractMediaDataPtr& mediaData)
{
    if (m_gotKeyDataInfo.size() <= mediaData->channelNumber)
        m_gotKeyDataInfo.resize(mediaData->channelNumber + 1);
    if (mediaData->dataType == QnAbstractMediaData::VIDEO)
    {
        if (mediaData->flags & AV_PKT_FLAG_KEY)
            m_gotKeyDataInfo[mediaData->channelNumber] = true;
        return m_gotKeyDataInfo[mediaData->channelNumber];
    }
    return true;
}

QnAbstractMediaDataPtr QnMulticodecRtpReader::getNextData()
{
    if (!isStreamOpened())
        return QnAbstractMediaDataPtr(0);

    QnAbstractMediaDataPtr result;
    do {
        if (m_RtpSession.getTransport() == RTPSession::TRANSPORT_UDP)
            result = getNextDataUDP();
        else
            result = getNextDataTCP();

    } while(result && !gotKeyData(result));
    
    if (result)
        m_gotSomeFrame = true;

    if (!result && m_RtpSession.isOpened() && !m_pleaseStop && m_gotSomeFrame) 
    {
        NX_LOG(QString(lit("RTP read timeout for camera %1. Reopen stream")).arg(getResource()->getUniqueId()), cl_logWARNING);

        int elapsed = m_dataTimer.elapsed();
        QString reasonParamsEncoded;
        QnBusiness::EventReason reason;
        if (elapsed > MAX_FRAME_DURATION*2) {
            reason = QnBusiness::NetworkNoFrameReason;
            reasonParamsEncoded = QnNetworkIssueBusinessEvent::encodeTimeoutMsecs(elapsed);
        }
        else {
            reason = QnBusiness::NetworkConnectionClosedReason;
            reasonParamsEncoded = QnNetworkIssueBusinessEvent::encodePrimaryStream(m_role != Qn::CR_SecondaryLiveVideo);
        }
        emit networkIssue(getResource(),
            qnSyncTime->currentUSecsSinceEpoch(),
            reason,
            reasonParamsEncoded);
        QnVirtualCameraResource* cam = dynamic_cast<QnVirtualCameraResource*>(getResource().data());
        if (cam)
            cam->issueOccured();

    }

    return result;
}

void QnMulticodecRtpReader::processTcpRtcp(RTPIODevice* ioDevice, quint8* buffer, int bufferSize, int bufferCapacity)
{
    bool gotValue = false;
    RtspStatistic stats = m_RtpSession.parseServerRTCPReport(buffer+4, bufferSize-4, &gotValue);
    if (gotValue) {
        if (ioDevice->getSSRC() == 0 || ioDevice->getSSRC() == stats.ssrc)
            ioDevice->setStatistic(stats);
    }
    
    int outBufSize = m_RtpSession.buildClientRTCPReport(buffer+4, bufferCapacity-4);
    if (outBufSize > 0)
    {
        quint16* sizeField = (quint16*) (buffer+2);
        *sizeField = htons(outBufSize);
        m_RtpSession.sendBynaryResponse(buffer, outBufSize+4);
    }
    m_rtcpReportTimer.restart();
}

void QnMulticodecRtpReader::buildClientRTCPReport(quint8 chNumber)
{
    quint8 buffer[1024*4];
    buffer[0] = '$';
    buffer[1] = chNumber;
    int outBufSize = m_RtpSession.buildClientRTCPReport(buffer+4, sizeof(buffer)-4);
    if (outBufSize > 0)
    {
        quint16* sizeField = (quint16*) (buffer+2);
        *sizeField = htons(outBufSize);
        m_RtpSession.sendBynaryResponse(buffer, outBufSize+4);
    }
}

QnAbstractMediaDataPtr QnMulticodecRtpReader::getNextDataInternal()
{
    int size = m_tracks.size();
    for (int i = 0; i < size; ++i)
    {
        if (m_tracks[i].parser) {
            QnAbstractMediaDataPtr result = m_tracks[i].parser->nextData();
            if (result) {
                result->channelNumber = m_tracks[i].parser->logicalChannelNum();
                return result;
            }
        }
    }
    return QnAbstractMediaDataPtr();
}

QnAbstractMediaDataPtr QnMulticodecRtpReader::getNextDataTCP()
{
    // int readed;
    int errorRetryCnt = 0;
    int rtpChannelNum = -1;

    m_dataTimer.restart();

    while (m_RtpSession.isOpened() && !m_pleaseStop && m_dataTimer.elapsed() <= MAX_FRAME_DURATION*2)
    {
        while (m_gotData) 
        {
            QnAbstractMediaDataPtr data = getNextDataInternal();
            if (data)
                return data;
            else
                m_gotData = false;
        }

        int readed = m_RtpSession.readBinaryResponce(m_demuxedData, rtpChannelNum);
        m_RtpSession.sendKeepAliveIfNeeded();
        if (readed < 1)
            break; // error
        
        RTPSession::TrackType format = m_RtpSession.getTrackTypeByRtpChannelNum(rtpChannelNum);
        int channelNum = m_RtpSession.getChannelNum(rtpChannelNum);
        QnRtpStreamParser* parser = m_tracks[channelNum].parser;
        RTPIODevice* ioDevice = m_tracks[channelNum].ioDevice;
        if (m_tracks.size() < channelNum || !parser)
            continue;

        int rtpBufferOffset = m_demuxedData[rtpChannelNum]->size() - readed;

        if (format == RTPSession::TT_VIDEO || format == RTPSession::TT_AUDIO) 
        {
            if (!parser->processData((quint8*)m_demuxedData[rtpChannelNum]->data(), rtpBufferOffset+4, readed-4, ioDevice->getStatistic(), m_gotData))
            {
                clearKeyData(parser->logicalChannelNum());
                m_demuxedData[rtpChannelNum]->clear();
                if (++errorRetryCnt > RTSP_RETRY_COUNT) {
                    qWarning() << "Too many RTP errors for camera " << getResource()->getName() << ". Reopen stream";
                    closeStream();
                    return QnAbstractMediaDataPtr(0);
                }
            }
            else if (m_gotData) {
                m_demuxedData[rtpChannelNum]->clear();
            }
        }
        else if (format == RTPSession::TT_VIDEO_RTCP || format == RTPSession::TT_AUDIO_RTCP)
        {
            processTcpRtcp(ioDevice, (quint8*) m_demuxedData[rtpChannelNum]->data(), readed, m_demuxedData[rtpChannelNum]->capacity());
            m_demuxedData[rtpChannelNum]->clear();
        }
        else {
            m_demuxedData[rtpChannelNum]->clear();
        }

        if (m_rtcpReportTimer.elapsed() >= RTCP_REPORT_TIMEOUT) {
            foreach(const TrackInfo& track, m_tracks) {
                if (track.ioDevice)
                    buildClientRTCPReport(track.ioDevice->getRtcpTrackNum());
            }
            m_rtcpReportTimer.restart();
        }

    }

    return  QnAbstractMediaDataPtr();
}

static const int MAX_MEDIA_SOCKET_COUNT = 5;
static const int MEDIA_DATA_READ_TIMEOUT_MS = 100;

QnAbstractMediaDataPtr QnMulticodecRtpReader::getNextDataUDP()
{
    int readed;
    int errorRetryCount = 0;

    pollfd mediaSockPollArray[MAX_MEDIA_SOCKET_COUNT];
    memset( mediaSockPollArray, 0, sizeof(mediaSockPollArray) );

    m_dataTimer.restart();

    while (m_RtpSession.isOpened() && !m_pleaseStop && m_dataTimer.elapsed() <= MAX_FRAME_DURATION*2)
    {
        while (m_gotData) 
        {
            QnAbstractMediaDataPtr data = getNextDataInternal();
            if (data)
                return data;
            else
                m_gotData = false;
        }

        int nfds = 0;
        foreach(const TrackInfo& track, m_tracks) {
            if(track.ioDevice) {
                mediaSockPollArray[nfds].fd = track.ioDevice->getMediaSocket()->handle();
                mediaSockPollArray[nfds++].events = POLLIN;
            }
        }

        const int rez = poll( mediaSockPollArray, nfds, MEDIA_DATA_READ_TIMEOUT_MS );
        if (rez < 1) 
            continue;
        for( int i = 0; i < rez; ++i )
        {
            int tracksSize = m_tracks.size();
            for (int rtpChannelNum = 0; rtpChannelNum < tracksSize; ++rtpChannelNum)
            {
                if( m_tracks[rtpChannelNum].ioDevice && mediaSockPollArray[i].fd == m_tracks[rtpChannelNum].ioDevice->getMediaSocket()->handle())
                {
                    TrackInfo& track = m_tracks[rtpChannelNum];

                    quint8* rtpBuffer = RTPSession::prepareDemuxedData(m_demuxedData, rtpChannelNum, MAX_RTP_PACKET_SIZE); // todo: update here
                    readed = track.ioDevice->read( (char*) rtpBuffer, MAX_RTP_PACKET_SIZE);
                    if (readed < 1)
                        break;
                    m_demuxedData[rtpChannelNum]->finishWriting(readed);
                    quint8* bufferBase = (quint8*) m_demuxedData[rtpChannelNum]->data();
                    bool gotData = false;
                    if (!track.parser->processData(bufferBase, rtpBuffer-bufferBase, readed, track.ioDevice->getStatistic(), gotData)) 
                    {
                        clearKeyData(track.parser->logicalChannelNum());
                        m_demuxedData[rtpChannelNum]->clear();
                        if (++errorRetryCount > RTSP_RETRY_COUNT) {
                            qWarning() << "Too many RTP errors for camera " << getResource()->getName() << ". Reopen stream";
                            closeStream();
                            return QnAbstractMediaDataPtr(0);
                        }
                    }
                    if (gotData) {
                        m_gotData = true;
                        m_demuxedData[rtpChannelNum]->clear();
                    }
                }
            }
        }
        if (readed < 1)
            break;
    }

    return QnAbstractMediaDataPtr();
}

QnRtpStreamParser* QnMulticodecRtpReader::createParser(const QString& codecName)
{
    QnRtpStreamParser* result = 0;

    if (codecName.isEmpty())
        return 0;
    else if (codecName == QLatin1String("H264"))
        result = new CLH264RtpParser;
    else if (codecName == QLatin1String("JPEG"))
        result = new QnMjpegRtpParser;
    else if (codecName == QLatin1String("MPEG4-GENERIC"))
        result = new QnAacRtpParser;
    else if (codecName == QLatin1String("PCMU")) {
        QnSimpleAudioRtpParser* audioParser = new QnSimpleAudioRtpParser;
        audioParser->setCodecId(CODEC_ID_PCM_MULAW);
        result = audioParser;
    }
    else if (codecName == QLatin1String("PCMA")) {
        QnSimpleAudioRtpParser* audioParser = new QnSimpleAudioRtpParser;
        audioParser->setCodecId(CODEC_ID_PCM_ALAW);
        result = audioParser;
    }
    else if (codecName.startsWith(QLatin1String("G726"))) // g726-24, g726-32 e.t.c
    { 
        int bitRatePos = codecName.indexOf(QLatin1Char('-'));
        if (bitRatePos == -1)
            return 0;
        QString bitsPerSample = codecName.mid(bitRatePos+1);
        QnSimpleAudioRtpParser* audioParser = new QnSimpleAudioRtpParser;
        audioParser->setCodecId(CODEC_ID_ADPCM_G726);
        audioParser->setBitsPerSample(bitsPerSample.toInt()/8);
        audioParser->setSampleFormat(AV_SAMPLE_FMT_S16);
        result = audioParser;
    }
    else if (codecName == QLatin1String("L16")) {
        QnSimpleAudioRtpParser* audioParser = new QnSimpleAudioRtpParser;
        audioParser->setCodecId(CODEC_ID_PCM_S16BE);
        audioParser->setSampleFormat(AV_SAMPLE_FMT_S16);
        result = audioParser;
    }
    
    if (result)
        connect(result, SIGNAL(packetLostDetected(quint32, quint32)), this, SLOT(at_packetLost(quint32, quint32)));
    return result;
}

void QnMulticodecRtpReader::at_propertyChanged(const QnResourcePtr & /*res*/, const QString & key)
{
    if (key == QnMediaResource::rtpTransportKey())
        closeStream();
}

void QnMulticodecRtpReader::at_packetLost(quint32 prev, quint32 next)
{
    const QnResourcePtr& resource = getResource();
    QnVirtualCameraResource* cam = dynamic_cast<QnVirtualCameraResource*>(resource.data());
    if (cam)
        cam->issueOccured();

    emit networkIssue(resource,
                      qnSyncTime->currentUSecsSinceEpoch(),
                      QnBusiness::NetworkRtpPacketLossReason,
                      QnNetworkIssueBusinessEvent::encodePacketLossSequence(prev, next));

}

static int TCP_READ_BUFFER_SIZE = 512*1024;

CameraDiagnostics::Result QnMulticodecRtpReader::openStream()
{
    m_pleaseStop = false;
    if (isStreamOpened())
        return CameraDiagnostics::NoErrorResult();
    //m_timeHelper.reset();
    m_gotSomeFrame = false;
    QString transport;
    QVariant val;
    m_resource->getParam(QnMediaResource::rtpTransportKey(), val, QnDomainMemory);
    transport = val.toString();
    if (transport.isEmpty())
        transport = m_resource->getProperty(QnMediaResource::rtpTransportKey());

    if (transport.isEmpty()) {
        // if not defined, try transport from registry
        transport = defaultTransportToUse;
    }

    if (transport != RtpTransport::_auto && transport != RtpTransport::udp && transport != RtpTransport::tcp)
        transport = RtpTransport::_auto;

    m_RtpSession.setTransport(transport);
    if (transport != RtpTransport::udp)
        m_RtpSession.setTCPReadBufferSize(TCP_READ_BUFFER_SIZE);


    const QnNetworkResource* nres = dynamic_cast<QnNetworkResource*>(getResource().data());

    QString url;
    if (m_request.length() > 0)
    {
        if (m_request.startsWith(QLatin1String("rtsp://"))) {
            url = m_request;
        }
        else 
        {
            QTextStream(&url) << "rtsp://" << nres->getHostAddress() << ":" << nres->mediaPort();
            if (!m_request.startsWith(QLatin1Char('/')))
                url += QLatin1Char('/');
            url += m_request;;
        }
    }
    else
        QTextStream( &url ) << "rtsp://" << nres->getHostAddress() << ":" << nres->mediaPort();

    m_RtpSession.setAuth(nres->getAuth(), RTPSession::authBasic);
    m_tracks.clear();
    m_gotKeyDataInfo.clear();
    m_gotData = false;

    const CameraDiagnostics::Result result = m_RtpSession.open(url);
    if( result.errorCode != CameraDiagnostics::ErrorCode::noError )
        return result;

    QnVirtualCameraResourcePtr camera = qSharedPointerDynamicCast<QnVirtualCameraResource>(getResource());
    if (camera)
        m_RtpSession.setAudioEnabled(camera->isAudioEnabled());

    m_RtpSession.play(AV_NOPTS_VALUE, AV_NOPTS_VALUE, 1.0);

    m_numberOfVideoChannels = m_RtpSession.getTrackCount(RTPSession::TT_VIDEO);
    {
        QMutexLocker lock(&m_layoutMutex);
        m_customVideoLayout.clear();
        QString newVideoLayout;
        if (m_numberOfVideoChannels > 1) {
            m_customVideoLayout = QnCustomResourceVideoLayoutPtr(new QnCustomResourceVideoLayout(QSize(m_numberOfVideoChannels, 1)));
            for (int i = 0; i < m_numberOfVideoChannels; ++i)
                m_customVideoLayout->setChannel(i, 0, i); // arrange multi video layout from left to right
            newVideoLayout = m_customVideoLayout->toString();
        }

        QnVirtualCameraResourcePtr camRes = m_resource.dynamicCast<QnVirtualCameraResource>();
        if (camRes && m_role == Qn::CR_LiveVideo) {
            QVariant val;
            camRes->getParam(lit("VideoLayout"), val, QnDomainMemory);
            QString oldVideoLayout = val.toString();
            if (newVideoLayout != oldVideoLayout) {
                camRes->setParam(lit("VideoLayout"), newVideoLayout, QnDomainDatabase);
                camRes->saveParams();
            }
        }
    }
    
    RTPSession::TrackMap trackInfo =  m_RtpSession.getTrackInfo();
    m_tracks.resize(trackInfo.size());
    bool videoExist = false;
    bool audioExist = false;
    int logicalVideoNum = 0;
    for (int i = 0; i < trackInfo.size(); ++i)
    {
        RTPSession::TrackType trackType = trackInfo[i]->trackType;
        videoExist |= trackType == RTPSession::TT_VIDEO;
        audioExist |= trackType == RTPSession::TT_AUDIO;
        if (trackType == RTPSession::TT_VIDEO || trackType == RTPSession::TT_AUDIO) 
        {
            m_tracks[i].parser = createParser(trackInfo[i]->codecName.toUpper());
            if (m_tracks[i].parser) {
                m_tracks[i].parser->setTimeHelper(&m_timeHelper);
                m_tracks[i].parser->setSDPInfo(m_RtpSession.getSdpByTrackNum(trackInfo[i]->trackNum));
                m_tracks[i].ioDevice = trackInfo[i]->ioDevice;

                QnRtpAudioStreamParser* audioParser = dynamic_cast<QnRtpAudioStreamParser*> (m_tracks[i].parser);
                if (audioParser)
                    m_audioLayout = audioParser->getAudioLayout();

                if (trackType == RTPSession::TT_VIDEO)
                    m_tracks[i].parser->setLogicalChannelNum(logicalVideoNum++);
                else
                    m_tracks[i].parser->setLogicalChannelNum(m_numberOfVideoChannels);
            }
        }
    }

    if (m_role == Qn::CR_LiveVideo)
    {
        if (audioExist) {
            if (!camera->isAudioSupported())
                camera->forceEnableAudio();
        }
        else {
            camera->forceDisableAudio();
        }
    }

    m_rtcpReportTimer.restart();
    if (!audioExist && !videoExist) {
        m_RtpSession.stop();
        return CameraDiagnostics::NoMediaTrackResult( url );
    }

    return CameraDiagnostics::NoErrorResult();
}

int QnMulticodecRtpReader::getLastResponseCode() const
{
    return m_RtpSession.getLastResponseCode();
}

void QnMulticodecRtpReader::closeStream()
{
    m_RtpSession.sendTeardown();
    m_RtpSession.stop();
    for (unsigned int i = 0; i < m_demuxedData.size(); ++i) {
        if (m_demuxedData[i])
            m_demuxedData[i]->clear();
    }
}

bool QnMulticodecRtpReader::isStreamOpened() const
{
    return m_RtpSession.isOpened();
}

QnConstResourceAudioLayoutPtr QnMulticodecRtpReader::getAudioLayout() const
{
    return m_audioLayout;
}

void QnMulticodecRtpReader::pleaseStop()
{
    m_pleaseStop = true;
    m_RtpSession.stop();
}

void QnMulticodecRtpReader::setDefaultTransport( const RtpTransport::Value& _defaultTransportToUse )
{
    defaultTransportToUse = _defaultTransportToUse;
}

void QnMulticodecRtpReader::setRole(Qn::ConnectionRole role)
{
    m_role = role;
}

QnConstResourceVideoLayoutPtr QnMulticodecRtpReader::getVideoLayout() const
{
    QMutexLocker lock(&m_layoutMutex);
    return m_customVideoLayout;
}

#endif // ENABLE_DATA_PROVIDERS
