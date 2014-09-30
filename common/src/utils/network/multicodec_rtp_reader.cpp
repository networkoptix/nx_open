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
    m_audioIO(0),
    m_audioParser(0),
    m_timeHelper(res->getUniqueId()),
    m_pleaseStop(false),
    m_gotSomeFrame(false),
    m_role(Qn::CR_Default)
{
    memset(m_videoParsers, 0, sizeof(m_videoParsers));
    memset(m_videoIOs, 0, sizeof(m_videoIOs));
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
    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
        delete m_videoParsers[i];
    delete m_audioParser;
    for (unsigned int i = 0; i < m_demuxedData.size(); ++i)
        delete m_demuxedData[i];
}

void QnMulticodecRtpReader::setRequest(const QString& request)
{
    m_request = request;
}

void QnMulticodecRtpReader::setNeedKeyData()
{
    for (int i = 0; i < m_gotKeyData.size(); ++i)
        m_gotKeyData[i] = false;
}

void QnMulticodecRtpReader::checkIfNeedKeyData()
{
    if (m_lastVideoData)
    {
        Q_ASSERT_X(m_lastVideoData->channelNumber < (quint32)m_gotKeyData.size(), Q_FUNC_INFO, "Invalid channel number");
        if (m_lastVideoData->channelNumber < (quint32)m_gotKeyData.size())
        {
            if (m_lastVideoData->flags & AV_PKT_FLAG_KEY)
                m_gotKeyData[m_lastVideoData->channelNumber] = true;
             if (!m_gotKeyData[m_lastVideoData->channelNumber])
                 m_lastVideoData.clear();
        }
    }
}

QnAbstractMediaDataPtr QnMulticodecRtpReader::getNextData()
{
    if (!isStreamOpened())
        return QnAbstractMediaDataPtr(0);

    if (m_RtpSession.getTransport() == RTPSession::TRANSPORT_UDP)
        return getNextDataUDP();
    else
        return getNextDataTCP();
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

QnAbstractMediaDataPtr QnMulticodecRtpReader::getNextDataTCP()
{
    QnAbstractMediaDataPtr result;
    // int readed;
    int audioRetryCount = 0;
    int videoRetryCount = 0;
    int rtpChannelNum = -1;

    QElapsedTimer dataTimer;
    dataTimer.restart();

    while (m_RtpSession.isOpened() && !m_pleaseStop && !m_lastVideoData && m_lastAudioData.isEmpty() && dataTimer.elapsed() <= MAX_FRAME_DURATION*2)
    {
        int readed = m_RtpSession.readBinaryResponce(m_demuxedData, rtpChannelNum);
        m_RtpSession.sendKeepAliveIfNeeded();
        if (readed < 1)
            break; // error
        RTPSession::TrackType format = m_RtpSession.getTrackTypeByRtpChannelNum(rtpChannelNum);
        int channelNum = m_RtpSession.getChannelNum(rtpChannelNum);
        int rtpBufferOffset = m_demuxedData[rtpChannelNum]->size() - readed;

        if (format == RTPSession::TT_VIDEO && m_videoParsers[channelNum]) 
        {
            if (!m_videoParsers[channelNum]->processData((quint8*)m_demuxedData[rtpChannelNum]->data(), rtpBufferOffset+4, readed-4, m_videoIOs[channelNum]->getStatistic())) 
            {
                setNeedKeyData();
                m_demuxedData[rtpChannelNum]->clear();
                if (++videoRetryCount > RTSP_RETRY_COUNT) {
                    qWarning() << "Too many RTP errors for camera " << getResource()->getName() << ". Reopen stream";
                    closeStream();
                    return QnAbstractMediaDataPtr(0);
                }
            }
            m_lastVideoData = m_videoParsers[channelNum]->nextData();
            if (m_lastVideoData) {
                m_lastVideoData->channelNumber = channelNum;
                checkIfNeedKeyData();
                if (!m_lastVideoData)
                    m_demuxedData[rtpChannelNum]->clear();
            }
        }
        else if (format == RTPSession::TT_VIDEO_RTCP && m_videoParsers[channelNum])
        {
            processTcpRtcp(m_videoIOs[channelNum], (quint8*) m_demuxedData[rtpChannelNum]->data(), readed, m_demuxedData[rtpChannelNum]->capacity());
            m_demuxedData[rtpChannelNum]->clear();
        }
        else if (format == RTPSession::TT_AUDIO && m_audioParser) 
        {
            if (!m_audioParser->processData((quint8*)m_demuxedData[rtpChannelNum]->data(), rtpBufferOffset+4, readed-4, m_audioIO->getStatistic())) 
            {
                m_demuxedData[rtpChannelNum]->clear();
                if (++audioRetryCount > RTSP_RETRY_COUNT) {
                    closeStream();
                    return QnAbstractMediaDataPtr(0);
                }
            }
            else {
            }
        }
        else if (format == RTPSession::TT_AUDIO_RTCP && m_audioParser)
        {
            processTcpRtcp(m_audioIO, (quint8*)m_demuxedData[rtpChannelNum]->data(), readed, m_demuxedData[rtpChannelNum]->capacity());
            m_demuxedData[rtpChannelNum]->clear();
        }
        else {
            m_demuxedData[rtpChannelNum]->clear();
        }

        if (m_rtcpReportTimer.elapsed() >= RTCP_REPORT_TIMEOUT) {
            if (m_videoIOs[channelNum])
                buildClientRTCPReport(m_videoIOs[channelNum]->getRtcpTrackNum());
            if (m_audioIO)
                buildClientRTCPReport(m_audioIO->getRtcpTrackNum());
            m_rtcpReportTimer.restart();
        }

    }

    if (m_lastVideoData)
    {
        result = m_lastVideoData;
        m_lastVideoData.clear();
        if (rtpChannelNum >= 0)
            m_demuxedData[rtpChannelNum]->clear();
        m_gotSomeFrame = true;
        return result;
    }
    if (!m_lastAudioData.isEmpty())
    {
        result = m_lastAudioData[0];
        m_lastAudioData.removeAt(0);
        if (rtpChannelNum >= 0)
            m_demuxedData[rtpChannelNum]->clear();
        result->channelNumber += m_numberOfVideoChannels;

        m_gotSomeFrame = true;
        return result;
    }
    if (m_RtpSession.isOpened() && !m_pleaseStop && m_gotSomeFrame) 
    {
        NX_LOG(QString(lit("RTP read timeout for camera %1. Reopen stream")).arg(getResource()->getUniqueId()), cl_logWARNING);

        int elapsed = dataTimer.elapsed();
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

static const int MAX_MEDIA_SOCKET_COUNT = 2;
static const int MEDIA_DATA_READ_TIMEOUT_MS = 100;

QnAbstractMediaDataPtr QnMulticodecRtpReader::getNextDataUDP()
{
    QnAbstractMediaDataPtr result;
    int readed;
    int audioRetryCount = 0;
    int videoRetryCount = 0;

    pollfd mediaSockPollArray[MAX_MEDIA_SOCKET_COUNT];
    memset( mediaSockPollArray, 0, sizeof(mediaSockPollArray) );

    QElapsedTimer dataTimer;
    dataTimer.restart();

    while (m_RtpSession.isOpened() && !m_pleaseStop && !m_lastVideoData && m_lastAudioData.isEmpty() && dataTimer.elapsed() <= MAX_FRAME_DURATION*2)
    {
        int nfds = 0;
        for (int channelNum = 0; channelNum < CL_MAX_CHANNELS; ++channelNum) {
            if(m_videoIOs[channelNum]) {
                mediaSockPollArray[nfds].fd = m_videoIOs[channelNum]->getMediaSocket()->handle();
                mediaSockPollArray[nfds++].events = POLLIN;
            }
        }
        if(m_audioIO) {
            mediaSockPollArray[nfds].fd = m_audioIO->getMediaSocket()->handle();
            mediaSockPollArray[nfds++].events = POLLIN;
        }

        const int rez = poll( mediaSockPollArray, nfds, MEDIA_DATA_READ_TIMEOUT_MS );
        if (rez < 1) 
            continue;
        for( int i = 0; i < rez; ++i )
        {
            for (int channelNum = 0; channelNum < CL_MAX_CHANNELS; ++channelNum)
            {
                if( m_videoIOs[channelNum] && mediaSockPollArray[i].fd == m_videoIOs[channelNum]->getMediaSocket()->handle())
                {
                    int rtpChannelNum = channelNum;

                    quint8* rtpBuffer = RTPSession::prepareDemuxedData(m_demuxedData, rtpChannelNum, MAX_RTP_PACKET_SIZE); // todo: update here
                    readed = m_videoIOs[0]->read( (char*) rtpBuffer, MAX_RTP_PACKET_SIZE);
                    if (readed < 1)
                        break;
                    m_demuxedData[rtpChannelNum]->finishWriting(readed);
                    quint8* bufferBase = (quint8*) m_demuxedData[rtpChannelNum]->data();
                    if (!m_videoParsers[channelNum]->processData(bufferBase, rtpBuffer-bufferBase, readed, m_videoIOs[channelNum]->getStatistic())) 
                    {
                        setNeedKeyData();
                        m_demuxedData[rtpChannelNum]->clear();
                        if (++videoRetryCount > RTSP_RETRY_COUNT) {
                            qWarning() << "Too many RTP errors for camera " << getResource()->getName() << ". Reopen stream";
                            closeStream();
                            return QnAbstractMediaDataPtr(0);
                        }
                    }
                    m_lastVideoData = m_videoParsers[channelNum]->nextData();
                    if (m_lastVideoData) 
                    {
                        m_lastVideoData->channelNumber = channelNum;
                        checkIfNeedKeyData();
                        if (!m_lastVideoData)
                            m_demuxedData[rtpChannelNum]->clear();
                    }
                }
            }
            if( m_audioIO && mediaSockPollArray[i].fd == m_audioIO->getMediaSocket()->handle() )
            {
                int rtpChannelNum = m_numberOfVideoChannels;

                quint8* rtpBuffer = RTPSession::prepareDemuxedData(m_demuxedData, rtpChannelNum, MAX_RTP_PACKET_SIZE); // todo: update here
                readed = m_audioIO->read( (char*) rtpBuffer, MAX_RTP_PACKET_SIZE);
                if (readed < 1)
                    break;
                m_demuxedData[rtpChannelNum]->finishWriting(readed);
                quint8* bufferBase = (quint8*) m_demuxedData[rtpChannelNum]->data();
                if (!m_audioParser->processData(bufferBase, rtpBuffer-bufferBase, readed, m_audioIO->getStatistic())) 
                {
                    m_demuxedData[rtpChannelNum]->clear();
                    if (++audioRetryCount > RTSP_RETRY_COUNT) {
                        closeStream();
                        return QnAbstractMediaDataPtr(0);
                    }
                }
            }
        }
        if (readed < 1)
            break;
    }

    if (m_lastVideoData)
    {
        result = m_lastVideoData;
        m_lastVideoData.clear();
        m_demuxedData[result->channelNumber]->clear();
        return result;
    }
    if (!m_lastAudioData.isEmpty())
    {
        result = m_lastAudioData[0];
        m_lastAudioData.removeAt(0);
        result->channelNumber += m_numberOfVideoChannels;
        m_demuxedData[result->channelNumber]->clear();

        return result;
    }

    NX_LOG(QString(lit("RTP read timeout for camera %1. Reopen stream")).arg(getResource()->getUniqueId()), cl_logWARNING);
    return result;
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

void QnMulticodecRtpReader::at_propertyChanged(const QnResourcePtr & res, const QString & key)
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


void QnMulticodecRtpReader::initIO(RTPIODevice** ioDevice, QnRtpStreamParser* parser, const RTPSession::TrackType mediaType, int channelNum)
{
    *ioDevice = 0;
    if (parser)
    {
        *ioDevice = m_RtpSession.getTrackIoByType(mediaType, channelNum);
        parser->setSDPInfo(m_RtpSession.getSdpByType(mediaType, channelNum));
    }
    /*
    else {
        if (!m_RtpSession.getCodecNameByType(mediaType).isEmpty())
            qWarning() << "Unsupported RTSP " << m_RtpSession.mediaTypeToStr(mediaType) << " codec" << m_RtpSession.getCodecNameByType(mediaType);
    }
    */
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
    
    for (int i = 0; i < CL_MAX_CHANNELS; ++i) {
        delete m_videoParsers[i];
        m_videoParsers[i] = 0;
        m_videoIOs[i] = 0;
    }

    delete m_audioParser;
    m_audioParser = 0;
    m_audioIO = 0;

    const CameraDiagnostics::Result result = m_RtpSession.open(url);
    if( result.errorCode != CameraDiagnostics::ErrorCode::noError )
        return result;

    QnVirtualCameraResourcePtr camera = qSharedPointerDynamicCast<QnVirtualCameraResource>(getResource());
    if (camera)
        m_RtpSession.setAudioEnabled(camera->isAudioEnabled());

    m_RtpSession.play(AV_NOPTS_VALUE, AV_NOPTS_VALUE, 1.0);

    m_numberOfVideoChannels = m_RtpSession.getTrackCount(RTPSession::TT_VIDEO);
    m_gotKeyData.resize(m_numberOfVideoChannels);

    {
        QMutexLocker lock(&m_layoutMutex);
        m_customVideoLayout.clear();
        m_gotKeyData.resize(m_numberOfVideoChannels);
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
    
    for (int i = 0; i < CL_MAX_CHANNELS; ++i) {
        m_videoParsers[i] = dynamic_cast<QnRtpVideoStreamParser*> (createParser(m_RtpSession.getCodecNameByType(RTPSession::TT_VIDEO, i).toUpper()));
        if (m_videoParsers[i]) {
            m_videoParsers[i]->setTimeHelper(&m_timeHelper);
            initIO(&m_videoIOs[i], m_videoParsers[i], RTPSession::TT_VIDEO, i);
        }
    }
    m_audioParser = dynamic_cast<QnRtpAudioStreamParser*> (createParser(m_RtpSession.getCodecNameByType(RTPSession::TT_AUDIO, m_numberOfVideoChannels).toUpper()));
    if (m_audioParser)
        m_audioParser->setTimeHelper(&m_timeHelper);

    initIO(&m_audioIO, m_audioParser, RTPSession::TT_AUDIO, m_numberOfVideoChannels);

    if (m_role == Qn::CR_LiveVideo)
    {
        if (m_audioIO) {
            if (!camera->isAudioSupported())
                camera->forceEnableAudio();
        }
        else {
            camera->forceDisableAudio();
        }
    }

    if (!m_videoIOs[0] && !m_audioIO)
        m_RtpSession.stop();
    m_rtcpReportTimer.restart();
    if( m_videoIOs[0] || m_audioIO )
        return CameraDiagnostics::NoErrorResult();
    else
        return CameraDiagnostics::NoMediaTrackResult( url );
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
    if (m_audioParser)
        return m_audioParser->getAudioLayout();
    else
        return QnConstResourceAudioLayoutPtr();
}

void QnMulticodecRtpReader::pleaseStop()
{
    m_pleaseStop = true;
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
