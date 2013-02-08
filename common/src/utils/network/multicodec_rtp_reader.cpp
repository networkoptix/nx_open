#include "multicodec_rtp_reader.h"
#include "utils/network/rtp_stream_parser.h"
#include "core/resource/network_resource.h"
#include "utils/network/h264_rtp_parser.h"
#include "aac_rtp_parser.h"
#include "utils/common/util.h"
#include "core/resource/resource_media_layout.h"
#include "core/resource/media_resource.h"
#include "simpleaudio_rtp_parser.h"
#include "mjpeg_rtp_parser.h"
#include "core/resource/camera_resource.h"
#include <business/business_event_connector.h>
#include "utils/common/synctime.h"

extern QSettings qSettings;
static const int RTSP_RETRY_COUNT = 6;
static const int RTCP_REPORT_TIMEOUT = 30 * 1000;

QnMulticodecRtpReader::QnMulticodecRtpReader(QnResourcePtr res):
    QnResourceConsumer(res),
    m_videoIO(0),
    m_audioIO(0),
    m_videoParser(0),
    m_audioParser(0),
    m_timeHelper(res->getUniqueId()),
    m_pleaseStop(false)
{
    QnNetworkResourcePtr netRes = qSharedPointerDynamicCast<QnNetworkResource>(res);
    if (netRes)
        m_RtpSession.setTCPTimeout(netRes->getNetworkTimeout());
    else
        m_RtpSession.setTCPTimeout(1000 * 5);
    QnMediaResourcePtr mr = qSharedPointerDynamicCast<QnMediaResource>(res);
    m_numberOfVideoChannels = mr->getVideoLayout()->numberOfChannels();
    m_gotKeyData.resize(m_numberOfVideoChannels);

    connect(this, SIGNAL(networkIssue(const QnResourcePtr&, qint64, const QString&)), qnBusinessRuleConnector, SLOT(at_networkIssue(const QnResourcePtr&, qint64, const QString&)));
}

QnMulticodecRtpReader::~QnMulticodecRtpReader()
{
    delete m_videoParser;
    delete m_audioParser;
    for (int i = 0; i < m_demuxedData.size(); ++i)
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
    int channelNum = 0;

    QTime dataTimer;
    dataTimer.restart();

    while (m_RtpSession.isOpened() && !m_pleaseStop && !m_lastVideoData && m_lastAudioData.isEmpty() && dataTimer.elapsed() <= MAX_FRAME_DURATION*2)
    {
        int readed = m_RtpSession.readBinaryResponce(m_demuxedData, channelNum);
        m_RtpSession.sendKeepAliveIfNeeded();
        if (readed < 1)
            break; // error
        RTPSession::TrackType format = m_RtpSession.getTrackTypeByRtpChannelNum(channelNum);
        int rtpBufferOffset = m_demuxedData[channelNum]->size() - readed;
        if (format == RTPSession::TT_VIDEO && m_videoParser) 
        {
            if (!m_videoParser->processData((quint8*)m_demuxedData[channelNum]->data(), rtpBufferOffset+4, readed-4, m_videoIO->getStatistic(), m_lastVideoData)) 
            {
                setNeedKeyData();
                m_demuxedData[channelNum]->clear();
                if (++videoRetryCount > RTSP_RETRY_COUNT) {
                    qWarning() << "Too many RTP errors for camera " << getResource()->getName() << ". Reopen stream";
                    closeStream();
                    return QnAbstractMediaDataPtr(0);
                }
            }
            else if (m_lastVideoData) {
                checkIfNeedKeyData();
                if (!m_lastVideoData)
                    m_demuxedData[channelNum]->clear();
            }
        }
        else if (format == RTPSession::TT_VIDEO_RTCP && m_videoParser)
        {
            processTcpRtcp(m_videoIO, (quint8*) m_demuxedData[channelNum]->data(), readed, m_demuxedData[channelNum]->capacity());
            m_demuxedData[channelNum]->clear();
        }
        else if (format == RTPSession::TT_AUDIO && m_audioParser) 
        {
            if (!m_audioParser->processData((quint8*)m_demuxedData[channelNum]->data(), rtpBufferOffset+4, readed-4, m_audioIO->getStatistic(), m_lastAudioData)) 
            {
                m_demuxedData[channelNum]->clear();
                if (++audioRetryCount > RTSP_RETRY_COUNT) {
                    closeStream();
                    return QnAbstractMediaDataPtr(0);
                }
            }
        }
        else if (format == RTPSession::TT_AUDIO_RTCP && m_audioParser)
        {
            processTcpRtcp(m_audioIO, (quint8*)m_demuxedData[channelNum]->data(), readed, m_demuxedData[channelNum]->capacity());
            m_demuxedData[channelNum]->clear();
        }
        else {
            m_demuxedData[channelNum]->clear();
        }

        if (m_rtcpReportTimer.elapsed() >= RTCP_REPORT_TIMEOUT) {
            if (m_videoIO)
                buildClientRTCPReport(m_videoIO->getRtcpTrackNum());
            if (m_audioIO)
                buildClientRTCPReport(m_audioIO->getRtcpTrackNum());
            m_rtcpReportTimer.restart();
        }

    }

    if (m_lastVideoData)
    {
        result = m_lastVideoData;
		m_lastVideoData.clear();
        m_demuxedData[channelNum]->clear();
        return result;
    }
    if (!m_lastAudioData.isEmpty())
    {
        result = m_lastAudioData[0];
        m_lastAudioData.removeAt(0);
        m_demuxedData[channelNum]->clear();
        result->channelNumber += m_numberOfVideoChannels;

        return result;
    }
    if (m_RtpSession.isOpened() && !m_pleaseStop) {
        qWarning() << "RTP read timeout for camera " << getResource()->getUniqueId() << ". Reopen stream";
        emit networkIssue(getResource(), qnSyncTime->currentUSecsSinceEpoch(), QString(QLatin1String("No video frame during %1 seconds")).arg(MAX_FRAME_DURATION*2/1000));
    }
    return result;
}


QnAbstractMediaDataPtr QnMulticodecRtpReader::getNextDataUDP()
{
    QnAbstractMediaDataPtr result;
    //quint8 rtpBuffer[MAX_RTP_PACKET_SIZE];
    int readed;
    int audioRetryCount = 0;
    int videoRetryCount = 0;

    fd_set read_set;
    timeval timeVal;
    timeVal.tv_sec  = 0;
    timeVal.tv_usec = 100*1000;


    QTime dataTimer;
    dataTimer.restart();

    while (m_RtpSession.isOpened() && !m_pleaseStop && !m_lastVideoData && m_lastAudioData.isEmpty() && dataTimer.elapsed() <= MAX_FRAME_DURATION*2)
    {
        int nfds = 0;
        FD_ZERO(&read_set);

        if(m_videoIO)  {
            FD_SET(m_videoIO->getMediaSocket()->handle(), &read_set);
            nfds = qMax(m_videoIO->getMediaSocket()->handle(), nfds);
        }
        if(m_audioIO) {
            FD_SET(m_audioIO->getMediaSocket()->handle(), &read_set);
            nfds = qMax(m_audioIO->getMediaSocket()->handle(), nfds);
        }
        nfds++;

        int rez = ::select(nfds, &read_set, NULL, NULL, &timeVal);
        if (rez < 1) 
            continue;
        if (m_videoIO && FD_ISSET(m_videoIO->getMediaSocket()->handle(), &read_set))
        {
            quint8* rtpBuffer = RTPSession::prepareDemuxedData(m_demuxedData, RTPSession::TT_VIDEO, MAX_RTP_PACKET_SIZE); // todo: update here
            readed = m_videoIO->read( (char*) rtpBuffer, MAX_RTP_PACKET_SIZE);
            if (readed < 1)
                break;
            m_demuxedData[RTPSession::TT_VIDEO]->finishWriting(readed);
            quint8* bufferBase = (quint8*) m_demuxedData[RTPSession::TT_VIDEO]->data();
            if (!m_videoParser->processData(bufferBase, rtpBuffer-bufferBase, readed, m_videoIO->getStatistic(), m_lastVideoData)) 
            {
                setNeedKeyData();
                m_demuxedData[RTPSession::TT_VIDEO]->clear();
                if (++videoRetryCount > RTSP_RETRY_COUNT) {
                    qWarning() << "Too many RTP errors for camera " << getResource()->getName() << ". Reopen stream";
                    closeStream();
                    return QnAbstractMediaDataPtr(0);
                }
            }
            else if (m_lastVideoData) 
            {
                checkIfNeedKeyData();
                if (!m_lastVideoData)
                    m_demuxedData[RTPSession::TT_VIDEO]->clear();
            }
        }

        if (m_audioIO && FD_ISSET(m_audioIO->getMediaSocket()->handle(), &read_set))
        {
            quint8* rtpBuffer = RTPSession::prepareDemuxedData(m_demuxedData, RTPSession::TT_AUDIO, MAX_RTP_PACKET_SIZE); // todo: update here
            readed = m_audioIO->read( (char*) rtpBuffer, MAX_RTP_PACKET_SIZE);
            if (readed < 1)
                break;
            m_demuxedData[RTPSession::TT_AUDIO]->finishWriting(readed);
            quint8* bufferBase = (quint8*) m_demuxedData[RTPSession::TT_AUDIO]->data();
            if (!m_audioParser->processData(bufferBase, rtpBuffer-bufferBase, readed, m_audioIO->getStatistic(), m_lastAudioData)) 
            {
                m_demuxedData[RTPSession::TT_AUDIO]->clear();
                if (++audioRetryCount > RTSP_RETRY_COUNT) {
                    closeStream();
                    return QnAbstractMediaDataPtr(0);
                }
            }
        }
    }

    if (m_lastVideoData)
    {
        result = m_lastVideoData;
        m_lastVideoData.clear();
        m_demuxedData[RTPSession::TT_VIDEO]->clear();
        return result;
    }
    if (!m_lastAudioData.isEmpty())
    {
        result = m_lastAudioData[0];
        m_lastAudioData.removeAt(0);
        m_demuxedData[RTPSession::TT_AUDIO]->clear();
        result->channelNumber += m_numberOfVideoChannels;

        return result;
    }

    qWarning() << "RTP read timeout for camera " << getResource()->getUniqueId() << ". Reopen stream";
    return result;
}

QnRtpStreamParser* QnMulticodecRtpReader::createParser(const QString& codecName)
{
    QnRtpStreamParser* result = 0;

    if (codecName == QLatin1String("H264"))
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
    
    if (result)
        connect(result, SIGNAL(packetLostDetected(quint32, quint32)), this, SLOT(at_packetLost(quint32, quint32)));
    return result;
}

void QnMulticodecRtpReader::at_packetLost(quint32 prev, quint32 next)
{
    emit networkIssue(getResource(), qnSyncTime->currentUSecsSinceEpoch(), QString(QLatin1String("RTP packet lost detected. prev seq.=%1 next seq.=%2")).arg(prev).arg(next));
}


void QnMulticodecRtpReader::initIO(RTPIODevice** ioDevice, QnRtpStreamParser* parser, const RTPSession::TrackType mediaType)
{
    *ioDevice = 0;
    if (parser)
    {
        *ioDevice = m_RtpSession.getTrackIoByType(mediaType);
        parser->setSDPInfo(m_RtpSession.getSdpByType(mediaType));
    }
    else {
        if (!m_RtpSession.getCodecNameByType(mediaType).isEmpty())
            qWarning() << "Unsupported RTSP " << m_RtpSession.mediaTypeToStr(mediaType) << " codec" << m_RtpSession.getCodecNameByType(mediaType);
    }
}

void QnMulticodecRtpReader::openStream()
{
    m_pleaseStop = false;
    if (isStreamOpened())
        return;
    //m_timeHelper.reset();

    QString transport = qSettings.value(QLatin1String("rtspTransport"), QLatin1String("AUTO")).toString().toUpper();
    if (transport != QLatin1String("AUTO") && transport != QLatin1String("UDP") && transport != QLatin1String("TCP"))
        transport = QLatin1String("AUTO");
    m_RtpSession.setTransport(transport);
    if (transport != QLatin1String("UDP"))
        m_RtpSession.setTCPReadBufferSize(1024*512);


    QnNetworkResourcePtr nres = getResource().dynamicCast<QnNetworkResource>();

    QString url;
    if (m_request.length() > 0)
    {
        if (m_request.startsWith(QLatin1String("rtsp://"))) {
            url = m_request;
        }
        else 
        {
            QTextStream(&url) << "rtsp://" << nres->getHostAddress();
            if (!m_request.startsWith(QLatin1Char('/')))
                url += QLatin1Char('/');
            url += m_request;;
        }
    }
    else
        QTextStream(&url) << "rtsp://" << nres->getHostAddress();

    m_RtpSession.setAuth(nres->getAuth());

    delete m_videoParser;
    m_videoParser = 0;
    delete m_audioParser;
    m_audioParser = 0;
    m_videoIO = m_audioIO = 0;

    if (m_RtpSession.open(url))
    {

        QnVirtualCameraResourcePtr camera = qSharedPointerDynamicCast<QnVirtualCameraResource>(getResource());
        if (camera)
            m_RtpSession.setAudioEnabled(camera->isAudioEnabled());

        m_RtpSession.play(AV_NOPTS_VALUE, AV_NOPTS_VALUE, 1.0);
        
        m_videoParser = dynamic_cast<QnRtpVideoStreamParser*> (createParser(m_RtpSession.getCodecNameByType(RTPSession::TT_VIDEO).toUpper()));
        if (m_videoParser)
            m_videoParser->setTimeHelper(&m_timeHelper);
        m_audioParser = dynamic_cast<QnRtpAudioStreamParser*> (createParser(m_RtpSession.getCodecNameByType(RTPSession::TT_AUDIO).toUpper()));
        if (m_audioParser)
            m_audioParser->setTimeHelper(&m_timeHelper);

        initIO(&m_videoIO, m_videoParser, RTPSession::TT_VIDEO);
        initIO(&m_audioIO, m_audioParser, RTPSession::TT_AUDIO);
        if (!m_videoIO && !m_audioIO)
            m_RtpSession.stop();
        m_rtcpReportTimer.restart();
    }
}

int QnMulticodecRtpReader::getLastResponseCode() const
{
    return m_RtpSession.getLastResponseCode();
}

void QnMulticodecRtpReader::closeStream()
{
    m_RtpSession.sendTeardown();
    m_RtpSession.stop();
}

bool QnMulticodecRtpReader::isStreamOpened() const
{
    return m_RtpSession.isOpened();
}

const QnResourceAudioLayout* QnMulticodecRtpReader::getAudioLayout() const
{
    if (m_audioParser)
        return m_audioParser->getAudioLayout();
    else
        return 0;
}

void QnMulticodecRtpReader::pleaseStop()
{
    m_pleaseStop = true;
}
