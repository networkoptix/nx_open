#include "audio_transcoder.h"

extern "C" {
#include <libswresample/swresample.h>
} // extern "C"

#include <nx/utils/log/log.h>

#include "camera/default_audio_encoder.h"
#include "ffmpeg/utils.h"

namespace nx::usb_cam::ffmpeg {

int AudioTranscoder::initializeDecoder(AVStream * stream)
{
    auto decoder = std::make_unique<ffmpeg::Codec>();

    int result = decoder->initializeDecoder(stream->codecpar);
    if (result < 0)
        return result;

    auto context = decoder->codecContext();
    context->request_channel_layout = ffmpeg::utils::suggestChannelLayout(decoder->codec());
    context->channel_layout = context->request_channel_layout;

    result = decoder->open();
    if (result < 0)
        return result;

    m_decoder = std::move(decoder);
    return 0;
}

int AudioTranscoder::initializeEncoder()
{
    int result = 0;
    auto encoder = getDefaultAudioEncoder(&result);
    if (result < 0)
        return result;

    result = encoder->open();
    if (result < 0)
        return result;

    m_encoder = std::move(encoder);
    return 0;
}

int AudioTranscoder::initializeResampledFrame()
{
    auto resampledFrame = std::make_unique<ffmpeg::Frame>();
    auto context = m_encoder->codecContext();

    static constexpr int kDefaultFrameSize = 2000;
    static constexpr int kDefaultAlignment =  32;

    int nbSamples = context->frame_size ? context->frame_size : kDefaultFrameSize;
    int result = resampledFrame->getBuffer(
        context->sample_fmt,
        nbSamples,
        context->channel_layout,
        kDefaultAlignment);
    if (result < 0)
        return result;

    resampledFrame->frame()->sample_rate = context->sample_rate;

    m_resampledFrame = std::move(resampledFrame);
    return result;
}

int AudioTranscoder::initalizeResampleContext(const AVFrame* frame)
{
    AVCodecContext * encoder = m_encoder->codecContext();

    m_resampleContext = swr_alloc_set_opts(
        nullptr,
        encoder->channel_layout,
        encoder->sample_fmt,
        encoder->sample_rate,
        frame->channel_layout,
        (AVSampleFormat)frame->format,
        frame->sample_rate,
        0,
        nullptr);

    if (!m_resampleContext)
        return AVERROR(ENOMEM);

    int result = swr_init(m_resampleContext);
    if (result < 0)
        swr_free(&m_resampleContext);

    return result;
}

int AudioTranscoder::initialize(AVStream * stream)
{
    int status = initializeDecoder(stream);
    if (status < 0)
        return status;

    status = initializeEncoder();
    if (status < 0)
        return status;

    status = initializeResampledFrame();
    if (status < 0)
        return status;

    status = m_adtsInjector.initialize(m_encoder.get());
    if (status < 0)
        return status;

    m_decodedFrame = std::make_unique<ffmpeg::Frame>();
    return status;
}

void AudioTranscoder::uninitialize()
{
    if (m_decoder)
        m_decoder->flush();
    m_decoder.reset(nullptr);

    if (m_resampleContext)
        swr_free(&m_resampleContext);

    if (m_encoder)
        m_encoder->flush();
    m_encoder.reset(nullptr);

    m_adtsInjector.uninitialize();
}

int AudioTranscoder::sendPacket(const ffmpeg::Packet& packet)
{
    return m_decoder->sendPacket(packet.packet());
}

bool AudioTranscoder::isFrameCached() const
{
    AVCodecContext * encoderCtx = m_encoder->codecContext();
    return m_resampleContext &&
        swr_get_delay(m_resampleContext, encoderCtx->sample_rate) >= encoderCtx->frame_size * 1.2;
}

int AudioTranscoder::receivePacket(std::shared_ptr<ffmpeg::Packet>& result)
{
    int status;
    if (isFrameCached())
    {
        // need to drain the the resampler periodically to avoid increasing audio delay
        status = resample(nullptr, m_resampledFrame->frame());
    }
    else
    {
        status = m_decoder->receiveFrame(m_decodedFrame->frame());
        if (status < 0)
            return status;
        status = resample(m_decodedFrame->frame(), m_resampledFrame->frame());
    }

    if (status < 0)
        return status;

    status = m_encoder->sendFrame(m_resampledFrame->frame());
    if (status < 0)
        return status;

    result = std::make_shared<ffmpeg::Packet>(m_encoder->codecId(), AVMEDIA_TYPE_AUDIO);
    status = m_encoder->receivePacket(result->packet());
    if (status < 0)
        return status;

    // Get duration before injecting into the packet because injection erases other fields.
    int64_t duration = result->packet()->duration;

    // Inject ADTS header into each packet becuase AAC encoder does not do it.
    status = m_adtsInjector.inject(result.get());
    if (status < 0)
        return status;

    result->packet()->duration = duration;
    return status;
}

int AudioTranscoder::resample(const AVFrame* frame, AVFrame* outFrame)
{
    if (!m_resampleContext)
    {
        int status = initalizeResampleContext(frame);
        if (status < 0)
            return status;
    }

    return swr_convert_frame(m_resampleContext, outFrame, frame);
}

} // namespace nx::usb_cam::ffmpeg
