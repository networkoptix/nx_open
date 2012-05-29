#include "multicodec_rtp_reader.h"
#include "utils/network/rtp_stream_parser.h"
#include "core/resource/network_resource.h"
#include "utils/network/h264_rtp_parser.h"
#include "aac_rtp_parser.h"
#include "utils/common/util.h"
#include "core/resource/resource_media_layout.h"
#include "core/resource/media_resource.h"
#include "simpleaudio_rtp_parser.h"

static const int RTSP_RETRY_COUNT = 3;

QnMulticodecRtpReader::QnMulticodecRtpReader(QnResourcePtr res):
QnResourceConsumer(res),
m_videoParser(0),
m_audioParser(0),
m_videoIO(0),
m_audioIO(0)
{
    QnNetworkResourcePtr netRes = qSharedPointerDynamicCast<QnNetworkResource>(res);
    if (netRes)
        m_RtpSession.setTCPTimeout(netRes->getNetworkTimeout());
    else
        m_RtpSession.setTCPTimeout(1000 * 2);
    QnMediaResourcePtr mr = qSharedPointerDynamicCast<QnMediaResource>(res);
    m_numberOfVideoChannels = mr->getVideoLayout()->numberOfChannels();
    
}

QnMulticodecRtpReader::~QnMulticodecRtpReader()
{
    delete m_videoParser;
    delete m_audioParser;
}

void QnMulticodecRtpReader::setRequest(const QString& request)
{
    m_request = request;
}

QnAbstractMediaDataPtr QnMulticodecRtpReader::getNextData()
{
    QnAbstractMediaDataPtr result;
    quint8 rtpBuffer[MAX_RTP_PACKET_SIZE];
    int readed;
    int audioRetryCount = 0;
    int videoRetryCount = 0;

    fd_set read_set;
    timeval timeVal;
    timeVal.tv_sec  = 0;
    timeVal.tv_usec = 100*1000;

    if (!isStreamOpened())
        return QnAbstractMediaDataPtr(0);

    QTime dataTimer;
    dataTimer.restart();

    while (m_RtpSession.isOpened() && m_lastVideoData.isEmpty() && m_lastAudioData.isEmpty() && dataTimer.elapsed() <= MAX_FRAME_DURATION)
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
            readed = m_videoIO->read( (char*) rtpBuffer, sizeof(rtpBuffer));
            if (readed < 1)
                break;
            if (!m_videoParser->processData(rtpBuffer, readed, m_lastVideoData)) {
                if (++videoRetryCount > RTSP_RETRY_COUNT) {
                    closeStream();
                    return QnAbstractMediaDataPtr(0);
                }
            }
        }

        if (m_audioIO && FD_ISSET(m_audioIO->getMediaSocket()->handle(), &read_set))
        {
            readed = m_audioIO->read( (char*) rtpBuffer, sizeof(rtpBuffer));
            if (readed < 1)
                break;
            if (!m_audioParser->processData(rtpBuffer, readed, m_lastAudioData)) {
                if (++audioRetryCount > RTSP_RETRY_COUNT) {
                    closeStream();
                    return QnAbstractMediaDataPtr(0);
                }
            }
        }
    }

    if (!m_lastVideoData.isEmpty())
    {
        result = m_lastVideoData[0];
        m_lastVideoData.removeAt(0);
        return result;
    }
    if (!m_lastAudioData.isEmpty())
    {
        result = m_lastAudioData[0];
        m_lastAudioData.removeAt(0);
        result->channelNumber += m_numberOfVideoChannels;

        return result;
    }

    return result;
}

QnRtpStreamParser* QnMulticodecRtpReader::createParser(const QString& codecName)
{
    if (codecName == "h264")
        return new CLH264RtpParser;
    else if (codecName == "mpeg4-generic")
        return new QnAacRtpParser;
    else if (codecName == "PCMA") {
        QnSimpleAudioRtpParser* result = new QnSimpleAudioRtpParser;
        result->setCodecId(CODEC_ID_PCM_MULAW);
        return result;
    }
    else if (codecName.startsWith("g726")) // g726-24, g726-32 e.t.c
    { 
        int bitRatePos = codecName.indexOf('-');
        if (bitRatePos == -1)
            return 0;
        QString bitsPerSample = codecName.mid(bitRatePos+1);
        QnSimpleAudioRtpParser* result = new QnSimpleAudioRtpParser;
        result->setCodecId(CODEC_ID_ADPCM_G726);
        result->setBitsPerSample(bitsPerSample.toInt()/8);
        result->setSampleFormat(AV_SAMPLE_FMT_S16);
        return result;
    }
    else
        return 0;
}

void QnMulticodecRtpReader::initIO(RTPIODevice** ioDevice, QnRtpStreamParser* parser, const QString& mediaType)
{
    if (parser)
    {
        *ioDevice = m_RtpSession.getTrackIoByType(mediaType);
        parser->setSDPInfo(m_RtpSession.getSdpByType(mediaType));
    }
    else {
        if (!m_RtpSession.getCodecNameByType(mediaType).isEmpty())
            qWarning() << "Unsupported RTSP " + mediaType + " codec" << m_RtpSession.getCodecNameByType(mediaType);
    }
}

void QnMulticodecRtpReader::openStream()
{
    if (isStreamOpened())
        return;

    QnNetworkResourcePtr nres = getResource().dynamicCast<QnNetworkResource>();

    QString url;
    if (m_request.length() > 0)
        QTextStream(&url) << "rtsp://" << nres->getHostAddress().toString() << "/" << m_request;
    else
        QTextStream(&url) << "rtsp://" << nres->getHostAddress().toString();

    m_RtpSession.setAuth(nres->getAuth());

    if (m_RtpSession.open(url))
    {
        m_RtpSession.play(AV_NOPTS_VALUE, AV_NOPTS_VALUE, 1.0);
        
        delete m_videoParser;
        m_videoParser = 0;
        delete m_audioParser;
        m_audioParser = 0;

        m_videoParser = createParser(m_RtpSession.getCodecNameByType("video"));
        m_audioParser = dynamic_cast<QnRtpAudioStreamParser*> (createParser(m_RtpSession.getCodecNameByType("audio")));

        initIO(&m_videoIO, m_videoParser, "video");
        initIO(&m_audioIO, m_audioParser, "audio");
    }

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

void QnMulticodecRtpReader::setDefaultAudioCodecName(const QString& value)
{
    m_RtpSession.setDefaultAudioCodecName(value);
}

const QnResourceAudioLayout* QnMulticodecRtpReader::getAudioLayout() const
{
    if (m_audioParser)
        return m_audioParser->getAudioLayout();
    else
        return 0;
}
