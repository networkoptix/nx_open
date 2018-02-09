#include "multicodec_rtp_reader.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <QtCore/QSettings>
#include <set>

#include <nx/vms/event/events/reasoned_event.h>
#include <nx/vms/event/events/network_issue_event.h>

#include <common/common_module.h>
#include <common/static_common_module.h>

#include <api/global_settings.h>
#include <api/app_server_connection.h>

#include <network/h264_rtp_parser.h>
#include <network/hevc_rtp_parser.h>
#include <network/aac_rtp_parser.h>
#include <network/simpleaudio_rtp_parser.h>
#include <network/mjpeg_rtp_parser.h>

#include <nx/streaming/rtp_stream_parser.h>
#include <nx/network/compat_poll.h>
#include <nx/utils/log/log.h>

#include <utils/common/synctime.h>
#include <utils/common/util.h>

#include <core/resource/network_resource.h>
#include <core/resource/resource_media_layout.h>
#include <core/resource/media_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_data_pool.h>

using namespace nx;

namespace {

static const int RTSP_RETRY_COUNT = 6;
static const int RTCP_REPORT_TIMEOUT = 30 * 1000;
static int SOCKET_READ_BUFFER_SIZE = 512*1024;

static const int MAX_MEDIA_SOCKET_COUNT = 5;
static const int MEDIA_DATA_READ_TIMEOUT_MS = 100;

static const std::chrono::seconds kNtpEpochTimeDiff(
    QDateTime::fromString(QLatin1String("1900-01-01"), Qt::ISODate)
        .secsTo(QDateTime::fromString(lit("1970-01-01"), Qt::ISODate)));

// prefix has the following format $<ChannelId(1byte)><PayloadLength(2bytes)>
static const int kInterleavedRtpOverTcpPrefixLength = 4;
static const int kOnvifNtpExtensionId = 0xabac;
static const int kOnvifNtpExtensionAltId = 0xabad;
static const int kOnvifNtpExtensionWordsNumber = 3;

} // namespace

namespace RtpTransport {

Value fromString(const QString& str)
{
    if (str == RtpTransport::udp)
        return RtpTransport::udp;
    else if (str == RtpTransport::tcp)
        return RtpTransport::tcp;
    else
        return RtpTransport::_auto;
}

static Value defaultTransportToUse( RtpTransport::_auto );

} // RtpTransport

QnMulticodecRtpReader::QnMulticodecRtpReader(
    const QnResourcePtr& res,
    std::unique_ptr<nx::network::AbstractStreamSocket> tcpSock)
:
    QnResourceConsumer(res),
    m_RtpSession(/*shouldGuessAuthDigest*/ false, std::move(tcpSock)),
    m_timeHelper(res->getUniqueId()),
    m_pleaseStop(false),
    m_gotSomeFrame(false),
    m_role(Qn::CR_Default),
    m_gotData(false),
    m_rtpStarted(false),
    m_prefferedAuthScheme(nx::network::http::header::AuthScheme::digest)
{
    const auto& globalSettings = res->commonModule()->globalSettings();
    m_rtpFrameTimeoutMs = globalSettings->rtpFrameTimeoutMs();
    m_maxRtpRetryCount = globalSettings->maxRtpRetryCount();

    m_RtpSession.setTCPTimeout(m_rtpFrameTimeoutMs);

    QnMediaResourcePtr mr = qSharedPointerDynamicCast<QnMediaResource>(res);
    m_numberOfVideoChannels = 1;
    m_customVideoLayout.clear();

    QnSecurityCamResourcePtr camRes = qSharedPointerDynamicCast<QnSecurityCamResource>(res);
    if (camRes)
        connect(this,       &QnMulticodecRtpReader::networkIssue, camRes.data(), &QnSecurityCamResource::networkIssue,              Qt::DirectConnection);
    Qn::directConnect(res.data(), &QnResource::propertyChanged, this, &QnMulticodecRtpReader::at_propertyChanged);

    auto securityCamResource = res.dynamicCast<QnSecurityCamResource>();
    if (securityCamResource)
    {
        auto resourceData = qnStaticCommon->dataPool()->data(securityCamResource);
        const bool ignoreCameraTimeIfBigJitter = resourceData.value<bool>(
            Qn::IGNORE_CAMERA_TIME_IF_BIG_JITTER_PARAM_NAME);
        if (ignoreCameraTimeIfBigJitter)
            m_timeHelper.setTimePolicy(TimePolicy::ignoreCameraTimeIfBigJitter);
    }
}

QnMulticodecRtpReader::~QnMulticodecRtpReader()
{
    directDisconnectAll();

    for (unsigned int i = 0; i < m_demuxedData.size(); ++i)
        delete m_demuxedData[i];
}

void QnMulticodecRtpReader::setRequest(const QString& request)
{
    m_request = request;
    calcStreamUrl();
}

void QnMulticodecRtpReader::clearKeyData(int channelNum)
{
    if (m_gotKeyDataInfo.size() > channelNum)
        m_gotKeyDataInfo[channelNum] = false;
}

bool QnMulticodecRtpReader::gotKeyData(const QnAbstractMediaDataPtr& mediaData)
{
    if ((size_t)m_gotKeyDataInfo.size() <= mediaData->channelNumber)
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

    const qint64 position = m_positionUsec.exchange(AV_NOPTS_VALUE);
    if (position != AV_NOPTS_VALUE)
    {
        m_RtpSession.sendPlay(position, AV_NOPTS_VALUE /*endTime */, m_RtpSession.getScale());
        createTrackParsers();
    }

    QnAbstractMediaDataPtr result;
    do {
        if (m_RtpSession.isTcpMode())
            result = getNextDataTCP();
        else
            result = getNextDataUDP();

    } while(result && !gotKeyData(result));

    if (result) {
        m_gotSomeFrame = true;
        return result;
    }

    if (!m_gotSomeFrame)
        return result; // if no frame received yet do not report network issue error

    bool isRtpFail = !m_RtpSession.isOpened() && m_rtpStarted;
    int elapsed = m_dataTimer.elapsed();
    if (isRtpFail || elapsed > m_rtpFrameTimeoutMs)
    {
        QString reasonParamsEncoded;
        vms::event::EventReason reason;
        if (elapsed > m_rtpFrameTimeoutMs) {
            reason = vms::event::EventReason::networkNoFrame;
            reasonParamsEncoded = vms::event::NetworkIssueEvent::encodeTimeoutMsecs(elapsed);
            NX_LOG(QString(lit("RTP read timeout for camera %1. Reopen stream")).arg(getResource()->getUniqueId()), cl_logWARNING);
        }
        else {
            reason = vms::event::EventReason::networkConnectionClosed;
            reasonParamsEncoded = vms::event::NetworkIssueEvent::encodePrimaryStream(m_role != Qn::CR_SecondaryLiveVideo);
            NX_LOG(QString(lit("RTP connection was forcibly closed by camera %1. Reopen stream")).arg(getResource()->getUniqueId()), cl_logWARNING);
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

void QnMulticodecRtpReader::processTcpRtcp(quint8* buffer, int bufferSize, int bufferCapacity)
{
    if (!m_RtpSession.processTcpRtcpData(buffer, bufferSize))
        NX_VERBOSE(this, "Can't parse RTCP report");
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
                if (result->dataType == QnAbstractMediaData::VIDEO)
                {
                    result->channelNumber =
                        std::min(result->channelNumber, (quint32) m_numberOfVideoChannels - 1);
                }
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

    while (m_RtpSession.isOpened() && !m_pleaseStop && m_dataTimer.elapsed() <= m_rtpFrameTimeoutMs)
    {
        while (m_gotData)
        {
            QnAbstractMediaDataPtr data = getNextDataInternal();
            if (data)
                return data;
            else
                m_gotData = false;
        }

        m_RtpSession.sendKeepAliveIfNeeded(); //< RTSP GET_PARAMETER
        // RTCP keep-alive packet
        if (m_rtcpReportTimer.elapsed() >= RTCP_REPORT_TIMEOUT)
        {
            for (const TrackInfo& track : m_tracks)
            {
                if (track.ioDevice)
                    buildClientRTCPReport(track.ioDevice->getRtcpTrackNum());
            }
            m_rtcpReportTimer.restart();
        }

        int bytesRead = m_RtpSession.readBinaryResponce(m_demuxedData, rtpChannelNum);
        if (bytesRead < 0 &&
            SystemError::getLastOSErrorCode() == SystemError::timedOut &&
            m_onSocketReadTimeoutCallback &&
            m_RtpSession.lastReceivedDataTimer().isValid() &&
            m_RtpSession.lastReceivedDataTimer().elapsed() < m_RtpSession.sessionTimeoutMs())
        {
            auto data = m_onSocketReadTimeoutCallback();
            if (data)
                return data;
            else
                continue;
        }

        if (bytesRead < 1)
            break; // error

        QnRtspClient::TrackType format = m_RtpSession.getTrackTypeByRtpChannelNum(rtpChannelNum);
        int trackNum = m_RtpSession.getChannelNum(rtpChannelNum);
        auto parser = m_tracks[trackNum].parser;
        QnRtspIoDevice* ioDevice = m_tracks[trackNum].ioDevice;
        if (m_tracks.size() < trackNum || !parser)
            continue;

        int rtpBufferOffset = m_demuxedData[rtpChannelNum]->size() - bytesRead;

        if (format == QnRtspClient::TT_VIDEO || format == QnRtspClient::TT_AUDIO)
        {
            const auto offset = rtpBufferOffset + kInterleavedRtpOverTcpPrefixLength;
            const auto length = bytesRead - kInterleavedRtpOverTcpPrefixLength;

            const auto rtcpStaticstics = rtspStatistics(offset, length, trackNum, rtpChannelNum);

            if (!parser->processData(
                (quint8*)m_demuxedData[rtpChannelNum]->data(),
                offset,
                length,
                rtcpStaticstics,
                m_gotData))
            {
                clearKeyData(parser->logicalChannelNum());
                m_demuxedData[rtpChannelNum]->clear();
                if (++errorRetryCnt > m_maxRtpRetryCount) {
                    qWarning() << "Too many RTP errors for camera " << getResource()->getName() << ". Reopen stream";
                    closeStream();
                    return QnAbstractMediaDataPtr(0);
                }
            }
            else if (m_gotData) {
                m_demuxedData[rtpChannelNum]->clear();
            }
        }
        else if (format == QnRtspClient::TT_VIDEO_RTCP || format == QnRtspClient::TT_AUDIO_RTCP)
        {
            processTcpRtcp((quint8*) m_demuxedData[rtpChannelNum]->data(), bytesRead, m_demuxedData[rtpChannelNum]->capacity());
            m_demuxedData[rtpChannelNum]->clear();
        }
        else {
            m_demuxedData[rtpChannelNum]->clear();
        }

    }

    return  QnAbstractMediaDataPtr();
}

QnAbstractMediaDataPtr QnMulticodecRtpReader::getNextDataUDP()
{
    int errorRetryCount = 0;

    pollfd mediaSockPollArray[MAX_MEDIA_SOCKET_COUNT];
    memset( mediaSockPollArray, 0, sizeof(mediaSockPollArray) );

    m_dataTimer.restart();

    while (m_RtpSession.isOpened() && !m_pleaseStop && m_dataTimer.elapsed() <= m_rtpFrameTimeoutMs)
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
        std::vector<int> fdToRtpIndex(m_tracks.size());
        fdToRtpIndex.clear();

        for (int i = 0; i < m_tracks.size(); ++i)
        {
            const TrackInfo& track = m_tracks[i];
            if(track.ioDevice)
            {
                mediaSockPollArray[nfds].fd = track.ioDevice->getMediaSocket()->handle();
                mediaSockPollArray[nfds++].events = POLLIN;
                fdToRtpIndex.push_back(i);
            }
        }

        const int rez = poll( mediaSockPollArray, nfds, MEDIA_DATA_READ_TIMEOUT_MS );
        if (rez < 1)
            continue;
        for( int i = 0; i < nfds; ++i )
        {
            if (!(mediaSockPollArray[i].revents & POLLIN ))
                continue;

            const int rtpChannelNum = fdToRtpIndex[i];
            TrackInfo& track = m_tracks[rtpChannelNum];
            bool gotData = false;
            while (!gotData)
            {
                quint8* rtpBuffer = QnRtspClient::prepareDemuxedData(m_demuxedData, rtpChannelNum, MAX_RTP_PACKET_SIZE); // todo: update here
                int bytesRead = track.ioDevice->read( (char*) rtpBuffer, MAX_RTP_PACKET_SIZE);
                if (bytesRead < 1)
                    break;
                m_demuxedData[rtpChannelNum]->finishWriting(bytesRead);
                quint8* bufferBase = (quint8*) m_demuxedData[rtpChannelNum]->data();

                const auto statistics = rtspStatistics(
                    rtpBuffer - bufferBase,
                    bytesRead,
                    rtpChannelNum,
                    rtpChannelNum);

                if (!track.parser->processData(
                        bufferBase,
                        rtpBuffer-bufferBase,
                        bytesRead,
                        statistics,
                        gotData))
                {
                    clearKeyData(track.parser->logicalChannelNum());
                    m_demuxedData[rtpChannelNum]->clear();
                    if (++errorRetryCount > m_maxRtpRetryCount) {
                        qWarning() << "Too many RTP errors for camera " << getResource()->getName() << ". Reopen stream";
                        closeStream();
                        return QnAbstractMediaDataPtr(0);
                    }
                }
            }
            if (gotData)
            {
                m_gotData = true;
                m_demuxedData[rtpChannelNum]->clear();
                break;
            }
        }
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
    else if (codecName == QLatin1String("H265"))
        result = new nx::network::rtp::HevcParser();
    else if (codecName == QLatin1String("JPEG"))
        result = new QnMjpegRtpParser;
    else if (codecName == QLatin1String("MPEG4-GENERIC"))
        result = new QnAacRtpParser;
    else if (codecName == QLatin1String("PCMU")) {
        QnSimpleAudioRtpParser* audioParser = new QnSimpleAudioRtpParser;
        audioParser->setCodecId(AV_CODEC_ID_PCM_MULAW);
        result = audioParser;
    }
    else if (codecName == QLatin1String("PCMA")) {
        QnSimpleAudioRtpParser* audioParser = new QnSimpleAudioRtpParser;
        audioParser->setCodecId(AV_CODEC_ID_PCM_ALAW);
        result = audioParser;
    }
    else if (codecName.startsWith(QLatin1String("G726"))) // g726-24, g726-32 e.t.c
    {
        int bitRatePos = codecName.indexOf(QLatin1Char('-'));
        if (bitRatePos == -1)
            return 0;
        QString bitsPerSample = codecName.mid(bitRatePos+1);
        QnSimpleAudioRtpParser* audioParser = new QnSimpleAudioRtpParser;
        audioParser->setCodecId(AV_CODEC_ID_ADPCM_G726);
        audioParser->setBitsPerSample(bitsPerSample.toInt()/8);
        audioParser->setSampleFormat(AV_SAMPLE_FMT_S16);
        result = audioParser;
    }
    else if (codecName == QLatin1String("L16")) {
        QnSimpleAudioRtpParser* audioParser = new QnSimpleAudioRtpParser;
        audioParser->setCodecId(AV_CODEC_ID_PCM_S16BE);
        audioParser->setSampleFormat(AV_SAMPLE_FMT_S16);
        result = audioParser;
    }

    if (result)
    {
        Qn::directConnect(result, &QnRtpStreamParser::packetLostDetected, this, &QnMulticodecRtpReader::at_packetLost);
    }
    return result;
}

void QnMulticodecRtpReader::at_propertyChanged(const QnResourcePtr & /*res*/, const QString & key)
{
    if (key == QnMediaResource::rtpTransportKey() && getRtpTransport() != m_RtpSession.getTransport())
        pleaseStop();
}

void QnMulticodecRtpReader::at_packetLost(quint32 prev, quint32 next)
{
    const QnResourcePtr& resource = getResource();
    QnVirtualCameraResource* cam = dynamic_cast<QnVirtualCameraResource*>(resource.data());
    if (cam)
        cam->issueOccured();

    emit networkIssue(resource,
                      qnSyncTime->currentUSecsSinceEpoch(),
                      vms::event::EventReason::networkRtpPacketLoss,
                      vms::event::NetworkIssueEvent::encodePacketLossSequence(prev, next));

}

QnRtspClient::TransportType QnMulticodecRtpReader::getRtpTransport() const
{
    QnRtspClient::TransportType result = QnRtspClient::TRANSPORT_AUTO;
    if (!m_resource)
        return result;

    QString transportStr = m_resource
        ->getProperty(QnMediaResource::rtpTransportKey())
            .toUpper()
            .trimmed();

    if (transportStr.isEmpty() || transportStr == RtpTransport::_auto)
        transportStr = m_rtpTransport;

    if (transportStr.isEmpty())
        transportStr = RtpTransport::defaultTransportToUse; // if not defined, try transport from registry

    transportStr = transportStr.toUpper().trimmed();
    if (transportStr == RtpTransport::udp)
        result = QnRtspClient::TRANSPORT_UDP;
    else if (transportStr == RtpTransport::tcp)
        result = QnRtspClient::TRANSPORT_TCP;

    return result;
}

void QnMulticodecRtpReader::setRtpTransport( const RtpTransport::Value& value )
{
    m_rtpTransport = value;
}


CameraDiagnostics::Result QnMulticodecRtpReader::openStream()
{
    m_pleaseStop = false;
    m_rtpStarted = false;
    if (isStreamOpened())
        return CameraDiagnostics::NoErrorResult();
    //m_timeHelper.reset();
    m_gotSomeFrame = false;
    m_RtpSession.setTransport(getRtpTransport());
    if (m_RtpSession.isTcpMode())
        m_RtpSession.setTCPReadBufferSize(SOCKET_READ_BUFFER_SIZE);


    const QnNetworkResource* nres = dynamic_cast<QnNetworkResource*>(getResource().data());

    calcStreamUrl();

    m_RtpSession.setAuth(nres->getAuth(), m_prefferedAuthScheme);

    m_tracks.clear();
    m_gotKeyDataInfo.clear();
    m_gotData = false;

    const qint64 position = m_positionUsec.exchange(AV_NOPTS_VALUE);
    const CameraDiagnostics::Result result = m_RtpSession.open(m_currentStreamUrl, position);
    if( result.errorCode != CameraDiagnostics::ErrorCode::noError )
        return result;

    if (m_RtpSession.isTcpMode())
        m_RtpSession.setTCPReadBufferSize(SOCKET_READ_BUFFER_SIZE);

    QnVirtualCameraResourcePtr camera = qSharedPointerDynamicCast<QnVirtualCameraResource>(getResource());
    if (camera)
        m_RtpSession.setAudioEnabled(camera->isAudioEnabled());

    m_RtpSession.play(position, AV_NOPTS_VALUE, m_RtpSession.getScale());

    m_numberOfVideoChannels = camera && camera->allowRtspVideoLayout() ?  m_RtpSession.getTrackCount(QnRtspClient::TT_VIDEO) : 1;
    {
        QnMutexLocker lock( &m_layoutMutex );
        m_customVideoLayout.clear();
        QString newVideoLayout;
        if (m_numberOfVideoChannels > 1) {
            m_customVideoLayout = QnCustomResourceVideoLayoutPtr(new QnCustomResourceVideoLayout(QSize(m_numberOfVideoChannels, 1)));
            for (int i = 0; i < m_numberOfVideoChannels; ++i)
                m_customVideoLayout->setChannel(i, 0, i); // arrange multi video layout from left to right

            newVideoLayout = m_customVideoLayout->toString();
            QnVirtualCameraResourcePtr camRes = m_resource.dynamicCast<QnVirtualCameraResource>();
            if (camRes && m_role == Qn::CR_LiveVideo)
                if (camRes->setProperty(Qn::VIDEO_LAYOUT_PARAM_NAME, newVideoLayout))
                    camRes->saveParams();
        }
    }

    createTrackParsers();

    bool videoExist = false;
    bool audioExist = false;
    int logicalVideoNum = 0;
    for (const auto& track: m_RtpSession.getTrackInfo())
    {
        QnRtspClient::TrackType trackType = track->trackType;
        videoExist |= trackType == QnRtspClient::TT_VIDEO;
        audioExist |= trackType == QnRtspClient::TT_AUDIO;
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
        return CameraDiagnostics::NoMediaTrackResult( m_currentStreamUrl );
    }
    m_rtpStarted = true;
    return CameraDiagnostics::NoErrorResult();
}

void QnMulticodecRtpReader::createTrackParsers()
{
    QnRtspClient::TrackMap trackInfo = m_RtpSession.getTrackInfo();
    m_tracks.resize(trackInfo.size());
    int logicalVideoNum = 0;
    for (int i = 0; i < trackInfo.size(); ++i)
    {
        QnRtspClient::TrackType trackType = trackInfo[i]->trackType;
        if (trackType == QnRtspClient::TT_VIDEO || trackType == QnRtspClient::TT_AUDIO)
        {
            m_tracks[i].parser.reset(createParser(trackInfo[i]->codecName.toUpper()));
            if (m_tracks[i].parser)
            {
                m_tracks[i].parser->setTimeHelper(&m_timeHelper);
                m_tracks[i].parser->setSdpInfo(m_RtpSession.getSdpByTrackNum(trackInfo[i]->trackNum));
                m_tracks[i].ioDevice = trackInfo[i]->ioDevice;

                auto secResource = m_resource.dynamicCast<QnSecurityCamResource>();
                if (secResource)
                {
                    auto resData = qnStaticCommon->dataPool()->data(secResource);
                    auto forceRtcpReports = resData.value<bool>(lit("forceRtcpReports"), false);

                    if (m_tracks[i].ioDevice)
                        m_tracks[i].ioDevice->setForceRtcpReports(forceRtcpReports);
                }

                QnRtpAudioStreamParser* audioParser = dynamic_cast<QnRtpAudioStreamParser*> (m_tracks[i].parser.get());
                if (audioParser)
                    m_audioLayout = audioParser->getAudioLayout();

                if (trackType == QnRtspClient::TT_VIDEO)
                    m_tracks[i].parser->setLogicalChannelNum(logicalVideoNum++);
                else
                    m_tracks[i].parser->setLogicalChannelNum(m_numberOfVideoChannels);

                if (!m_RtpSession.isTcpMode())
                {
                    m_tracks[i].ioDevice->getMediaSocket()->setRecvBufferSize(SOCKET_READ_BUFFER_SIZE);
                    m_tracks[i].ioDevice->getMediaSocket()->setNonBlockingMode(true);
                }
            }
        }
    }
}

int QnMulticodecRtpReader::getLastResponseCode() const
{
    return m_RtpSession.getLastResponseCode();
}

void QnMulticodecRtpReader::closeStream()
{
    m_rtpStarted = false;
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
    m_rtpStarted = false;
    m_RtpSession.shutdown();
}

void QnMulticodecRtpReader::setDefaultTransport( const RtpTransport::Value& value )
{
    RtpTransport::defaultTransportToUse = value;
}

void QnMulticodecRtpReader::setRole(Qn::ConnectionRole role)
{
    m_role = role;
}

void QnMulticodecRtpReader::setPrefferedAuthScheme(const nx::network::http::header::AuthScheme::Value scheme){
    m_prefferedAuthScheme = scheme;
}

QnConstResourceVideoLayoutPtr QnMulticodecRtpReader::getVideoLayout() const
{
    QnMutexLocker lock( &m_layoutMutex );
    return m_customVideoLayout;
}

void QnMulticodecRtpReader::setUserAgent(const QString& value)
{
    m_RtpSession.setUserAgent(value);
}

QString QnMulticodecRtpReader::getCurrentStreamUrl() const
{
    return m_currentStreamUrl;
}

void QnMulticodecRtpReader::calcStreamUrl()
{
    QString url;
    auto res = getResource();
    QnNetworkResourcePtr nres;

    m_currentStreamUrl.clear();

    if (res)
        nres = res.dynamicCast<QnNetworkResource>();
    else
        return;

    if (!nres)
        return;

    if (m_request.length() > 0)
    {
        if (m_request.startsWith(QLatin1String("rtsp://")))
        {
            m_currentStreamUrl = m_request;
        }
        else
        {
            QTextStream(&m_currentStreamUrl)
                << "rtsp://"
                << nres->getHostAddress() << ":"
                << nres->mediaPort();

            if (!m_request.startsWith(QLatin1Char('/')))
                m_currentStreamUrl += QLatin1Char('/');

            m_currentStreamUrl += m_request;;
        }
    }
    else
    {
        QTextStream(&m_currentStreamUrl) << "rtsp://" << nres->getHostAddress() << ":" << nres->mediaPort();
    }
}

void QnMulticodecRtpReader::setPositionUsec(qint64 value)
{
    m_positionUsec = value;
}

void QnMulticodecRtpReader::setDateTimeFormat(const QnRtspClient::DateTimeFormat& format)
{
    m_RtpSession.setDateTimeFormat(format);
}

void QnMulticodecRtpReader::setTrustToCameraTime(bool value)
{
    m_timeHelper.setTimePolicy(TimePolicy::forceCameraTime);
}

void QnMulticodecRtpReader::setTimePolicy(TimePolicy timePolicy)
{
    m_timeHelper.setTimePolicy(timePolicy);
}

void QnMulticodecRtpReader::addRequestHeader(
    const QString& requestName,
    const nx::network::http::HttpHeader& header)
{
    m_RtpSession.addRequestHeader(requestName, header);
}

QnRtspClient& QnMulticodecRtpReader::rtspClient()
{
    return m_RtpSession;
}

boost::optional<std::chrono::microseconds> QnMulticodecRtpReader::parseOnvifNtpExtensionTime(
    quint8* bufferStart,
    int length) const
{
    if (length < RtpHeader::RTP_HEADER_SIZE)
        return boost::none;

    const auto rtpHeader = (RtpHeader*) bufferStart;
    if (!rtpHeader->extension)
        return boost::none;

    if (length < RtpHeader::RTP_HEADER_SIZE + RtpHeaderExtension::kRtpExtensionHeaderLength)
        return boost::none;

    quint8* ptr = bufferStart + RtpHeader::RTP_HEADER_SIZE;
    const auto extensionId = htons(*(uint16_t*)ptr);

    if (!isOnvifNtpExtensionId(extensionId))
        return boost::none;

    const int extWords = ((int(ptr[2]) << 8) + ptr[3]);
    if (extWords < kOnvifNtpExtensionWordsNumber)
        return boost::none;

    const quint8* bufferEnd = bufferStart + length;
    if (ptr + kOnvifNtpExtensionWordsNumber * 4 > bufferEnd)
        return boost::none;

    ptr += RtpHeaderExtension::kRtpExtensionHeaderLength;
    const auto seconds = htonl(*(uint32_t*)ptr);
    const double fractions = htonl(*(uint32_t*)(ptr + 4));

    return std::chrono::microseconds(seconds * std::micro::den)
        + std::chrono::microseconds(
            (uint64_t)(fractions / std::numeric_limits<uint32_t>::max()
            * std::micro::den))
        - kNtpEpochTimeDiff;
}

QnRtspStatistic QnMulticodecRtpReader::rtspStatistics(
    int rtpBufferOffset,
    int rtpPacketSize,
    int track,
    int rtpChannel)
{
    if (m_tracks.size() - 1 < track)
        return QnRtspStatistic();

    auto ioDevice = m_tracks[track].ioDevice;
    if (!ioDevice)
        return QnRtspStatistic();

    auto rtcpStaticstics = ioDevice->getStatistic();
    const auto extensionNtpTime = parseOnvifNtpExtensionTime(
        (quint8*)m_demuxedData[rtpChannel]->data() + rtpBufferOffset,
        rtpPacketSize);

    if (extensionNtpTime.is_initialized())
        m_lastOnvifNtpExtensionTime = extensionNtpTime;

    rtcpStaticstics.ntpOnvifExtensionTime = m_lastOnvifNtpExtensionTime;

    return rtcpStaticstics;
}

bool QnMulticodecRtpReader::isOnvifNtpExtensionId(uint16_t id) const
{
    return id == kOnvifNtpExtensionId
        || id == kOnvifNtpExtensionAltId;
}

void QnMulticodecRtpReader::setOnSocketReadTimeoutCallback(OnSocketReadTimeoutCallback callback)
{
    m_onSocketReadTimeoutCallback = std::move(callback);
}

void QnMulticodecRtpReader::setRtpFrameTimeoutMs(int value)
{
    m_rtpFrameTimeoutMs = value;
}

#endif // ENABLE_DATA_PROVIDERS
