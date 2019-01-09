#include "onvif_audio_transmitter.h"
#include <nx/streaming/rtp/rtp.h>

namespace nx {
namespace vms::server {
namespace plugins {

OnvifAudioTransmitter::OnvifAudioTransmitter(QnVirtualCameraResource* res):
    QnAbstractAudioTransmitter(),
    m_resource(res)
{
    connect(
        m_resource, &QnResource::parentIdChanged, this,
        [this]()
        {
            pleaseStop();
        },
        Qt::DirectConnection);
}

OnvifAudioTransmitter::~OnvifAudioTransmitter()
{
    stop();
}

void OnvifAudioTransmitter::close()
{
    m_rtspConnection.reset();
    m_transcoder.reset();
    m_sequence = 0;
    m_firstPts = AV_NOPTS_VALUE;
}

void OnvifAudioTransmitter::prepare()
{
    close();
    QnRtspClient::Config config{/*shouldGuessAuthDigest*/ false, /*backChannelAudioOnly*/ true};
    m_rtspConnection.reset(new QnRtspClient(config));
    m_rtspConnection->setAuth(m_resource->getAuth(), nx::network::http::header::AuthScheme::digest);

    // DW cameras have issues with this attribute in current firmware. It change channel URL in SETUP response
    // if this attribute is specified.
    if (m_resource->resourceData().value<bool>("sendBackChannelAttribute", true))
        m_rtspConnection->setAdditionAttribute("Require", "www.onvif.org/ver20/backchannel");
    m_rtspConnection->setTransport(RtspTransport::tcp);

    const QString url = m_resource->sourceUrl(Qn::CR_LiveVideo);
    const CameraDiagnostics::Result result = m_rtspConnection->open(url);
    if (result.errorCode != CameraDiagnostics::ErrorCode::noError)
    {
        close();
        return;
    }

    if (!m_rtspConnection->play(DATETIME_NOW, AV_NOPTS_VALUE, 1.0))
    {
        close();
        return;
    }

    auto tracks = m_rtspConnection->getTrackInfo();
    if (tracks.empty())
    {
        close();
        return;
    }

    m_trackInfo = tracks[0];
    int defaultBitrate = 0;
    auto codecId = QnAbstractAudioTransmitter::toFfmpegCodec(m_trackInfo.sdpMedia.rtpmap.codecName, &defaultBitrate);
    m_transcoder.reset(new QnFfmpegAudioTranscoder(codecId));
    m_transcoder->setSampleRate(m_trackInfo.sdpMedia.rtpmap.clockRate);
    if (m_bitrateKbps > 0)
        m_transcoder->setBitrate(m_bitrateKbps);
    else if (defaultBitrate > 0)
        m_transcoder->setBitrate(defaultBitrate);
}

bool OnvifAudioTransmitter::processAudioData(const QnConstCompressedAudioDataPtr& audioData)
{
    if (!isInitialized())
    {
        prepare();
        if (!isInitialized())
            return false;
    }

    if (!m_transcoder->isOpened() && !m_transcoder->open(audioData->context))
    {
        close();
        return false;
    }

    QnAbstractMediaDataPtr transcoded;
    auto data = audioData;
    do
    {
        m_transcoder->transcodePacket(data, &transcoded);
        data.reset();

        if (!m_needStop && transcoded)
        {
            auto res = sendData(transcoded);

            if (!res)
            {
                close();
                break;
            }
        }

    } while (!m_needStop && transcoded);

    return true;
}

bool OnvifAudioTransmitter::sendData(const QnAbstractMediaDataPtr& audioData)
{
    QByteArray sendBuffer;
    const static int kRtpTcpHeaderSize = 4;
    sendBuffer.resize(audioData->dataSize() + nx::streaming::rtp::RtpHeader::kSize + kRtpTcpHeaderSize);
    char* rtpHeaderPtr = sendBuffer.data() + kRtpTcpHeaderSize;

    AVRational srcTimeBase = { 1, 1000000 };
    AVRational dstTimeBase = { 1, m_trackInfo.sdpMedia.rtpmap.clockRate};
    if (m_firstPts == AV_NOPTS_VALUE)
        m_firstPts = audioData->timestamp;
    auto timestamp = av_rescale_q(audioData->timestamp - m_firstPts, srcTimeBase, dstTimeBase);

    nx::streaming::rtp::buildRtpHeader(
        rtpHeaderPtr,
        0, //< ssrc
        audioData->dataSize(),
        timestamp,
        m_trackInfo.sdpMedia.payloadType,
        m_sequence++);

    sendBuffer.data()[0] = '$';
    sendBuffer.data()[1] = m_trackInfo.interleaved.first;
    quint16* lenPtr = (quint16*)(sendBuffer.data() + 2);
    *lenPtr = htons(sendBuffer.size() - kRtpTcpHeaderSize);
    memcpy(
        sendBuffer.data() + nx::streaming::rtp::RtpHeader::kSize + kRtpTcpHeaderSize,
        audioData->data(),
        audioData->dataSize());
    m_rtspConnection->sendBynaryResponse((const quint8*) sendBuffer.data(), sendBuffer.size());

    return true;
}

void OnvifAudioTransmitter::endOfRun()
{
    base_type::endOfRun();
    close();
}

void OnvifAudioTransmitter::setBitrateKbps(int value)
{
    m_bitrateKbps = value;
}

bool OnvifAudioTransmitter::isInitialized() const
{
    return m_rtspConnection && m_rtspConnection->isOpened();
}

bool OnvifAudioTransmitter::isCompatible(const QnAudioFormat& format) const
{
    return true;
}

} // namespace plugins
} // namespace vms::server
} // namespace nx
