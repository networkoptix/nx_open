#include "onvif_audio_transmitter.h"
#include <rtsp/rtsp_encoder.h>
#include <nx/streaming/rtp_stream_parser.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

AVCodecID toFfmpegCodec(const QString& codec)
{
    const auto lCodec = codec.toLower();
    if (lCodec == lit("aac"))
        return AV_CODEC_ID_AAC;
    else if (lCodec == lit("g726"))
        return AV_CODEC_ID_ADPCM_G726;
    else if (lCodec == lit("pcmu"))
        return AV_CODEC_ID_PCM_MULAW;
    else if (lCodec == lit("pcma"))
        return AV_CODEC_ID_PCM_ALAW;
    else
        return AV_CODEC_ID_NONE;
}

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
    m_trackInfo.reset();
    m_sequence = 0;
    m_firstPts = AV_NOPTS_VALUE;
}

void OnvifAudioTransmitter::prepare()
{
    close();
    m_rtspConnection.reset(new QnRtspClient(m_resource->toSharedPointer()));
    m_rtspConnection->setAuth(m_resource->getAuth(), nx_http::header::AuthScheme::digest);
    m_rtspConnection->setAdditionAttribute("Require", "www.onvif.org/ver20/backchannel");
    m_rtspConnection->setTransport(QnRtspClient::TRANSPORT_TCP);

    const QString url = m_resource->sourceUrl(Qn::CR_LiveVideo);
    const CameraDiagnostics::Result result = m_rtspConnection->open(url);
    if (result.errorCode != CameraDiagnostics::ErrorCode::noError)
    {
        close();
        return;
    }

    auto tracks = m_rtspConnection->getTrackInfo();
    tracks.erase(std::remove_if(tracks.begin(), tracks.end(),
        [this](const QSharedPointer<QnRtspClient::SDPTrackInfo>& track)
    {
        return !track->isBackChannel;
    }), tracks.end());
    if (tracks.isEmpty())
    {
        close();
        return;
    }
    tracks.resize(1);
    tracks[0]->trackNum = 0;
    tracks[0]->interleaved = QPair<int, int>(0, 1);

    m_rtspConnection->setTrackInfo(tracks);
    auto tracksToPlay = m_rtspConnection->play(DATETIME_NOW, AV_NOPTS_VALUE, 1.0);
    if (tracksToPlay.isEmpty())
    {
        close();
        return;
    }

    m_trackInfo = tracks[0];
    m_transcoder.reset(new QnFfmpegAudioTranscoder(toFfmpegCodec(m_trackInfo->codecName)));
    m_transcoder->setSampleRate(m_trackInfo->timeBase);
    if (m_bitrateKbps > 0)
        m_transcoder->setBitrate(m_bitrateKbps);
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
    sendBuffer.resize(audioData->dataSize() + RtpHeader::RTP_HEADER_SIZE + kRtpTcpHeaderSize);
    char* rtpHeaderPtr = sendBuffer.data() + kRtpTcpHeaderSize;
    
    AVRational srcTimeBase = { 1, 1000000 };
    AVRational dstTimeBase = { 1, m_trackInfo->timeBase };
    if (m_firstPts == AV_NOPTS_VALUE)
        m_firstPts = audioData->timestamp;
    auto timestamp = av_rescale_q(audioData->timestamp - m_firstPts, srcTimeBase, dstTimeBase);

    QnRtspEncoder::buildRTPHeader(
        rtpHeaderPtr, 
        0, //< ssrc
        audioData->dataSize(),
        timestamp,
        m_trackInfo->mapNum,
        m_sequence++);

    sendBuffer.data()[0] = '$';
    sendBuffer.data()[1] = m_trackInfo->interleaved.first;
    quint16* lenPtr = (quint16*)(sendBuffer.data() + 2);
    *lenPtr = htons(sendBuffer.size() - kRtpTcpHeaderSize);
    memcpy(
        sendBuffer.data() + RtpHeader::RTP_HEADER_SIZE + kRtpTcpHeaderSize,
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
} // namespace mediaserver_core
} // namespace nx
