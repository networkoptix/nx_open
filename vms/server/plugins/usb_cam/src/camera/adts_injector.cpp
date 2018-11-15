#include "adts_injector.h"

extern "C"{
#include <libavformat/avformat.h>
} // extern "C"

#include "ffmpeg/codec.h"
#include "ffmpeg/packet.h"

namespace nx{
namespace usb_cam {

namespace {

static constexpr const char * kOutputFormat = "adts";
static constexpr int kDefaultBufferSize = 512;

static int writePacket(void *opaque, uint8_t *buffer, int bufferSize)
{
    AdtsInjector * injector = static_cast<AdtsInjector*>(opaque);
    ffmpeg::Packet * packet = injector->currentPacket();

    packet->unreference();
    packet->newPacket(bufferSize);
    memcpy(packet->data(), buffer, bufferSize);

    return bufferSize;
}

} // namespace

AdtsInjector::~AdtsInjector()
{
    uninitialize();
}

int AdtsInjector::initialize(ffmpeg::Codec * codec)
{
    int result = avformat_alloc_output_context2(&m_formatContext, nullptr, kOutputFormat, nullptr);
    if (result < 0)
        return result;

    AVCodec * outputCodec = avcodec_find_encoder(m_formatContext->oformat->audio_codec);
    if (!outputCodec)
        return AVERROR_ENCODER_NOT_FOUND;

    m_outputStream = avformat_new_stream(m_formatContext, outputCodec);
    m_outputStream->id = m_formatContext->nb_streams - 1;

    result = avcodec_parameters_from_context(m_outputStream->codecpar, codec->codecContext());
    if(result < 0)
        return result;

    result = initializeIoContext(kDefaultBufferSize);
    if (result < 0)
        return result;

    m_formatContext->pb = m_ioContext;

    return avformat_write_header(m_formatContext, nullptr);
}

void AdtsInjector::uninitialize()
{
    if (m_formatContext)
    {
        av_write_trailer(m_formatContext);
        avformat_free_context(m_formatContext);
    }
    m_formatContext = nullptr;
    
    uninitializeIoContext();

    if(m_outputStream)
        avcodec_free_context(&m_outputStream->codec);
    m_outputStream = nullptr;
}

int AdtsInjector::inject(ffmpeg::Packet * packet)
{
    if(packet->size() > m_ioBufferSize)
    {
        int result = reinitializeIoContext(packet->size());
        if (result < 0)
            return result;
    }

    m_currentPacket = packet;
    return av_write_frame(m_formatContext, packet->packet());
}

ffmpeg::Packet * AdtsInjector::currentPacket()
{
    return m_currentPacket;
}

int AdtsInjector::initializeIoContext(int bufferSize)
{
    m_ioBufferSize = bufferSize;
    m_ioBuffer = (uint8_t*)av_malloc(m_ioBufferSize);
    if (!m_ioBuffer)
        return AVERROR(ENOMEM);

    m_ioContext = avio_alloc_context(
        m_ioBuffer,
        m_ioBufferSize,
        AVIO_FLAG_WRITE,
        this,
        nullptr,
        writePacket,
        nullptr);
    if (!m_ioContext)
        return AVERROR(ENOMEM);

    return 0;
}

void AdtsInjector::uninitializeIoContext()
{
    if(m_ioBuffer)
        av_free(m_ioBuffer);
    m_ioBuffer = nullptr;

    m_ioBufferSize = 0;

    if (m_ioContext)
        av_free(m_ioContext);
    m_ioContext = nullptr;

    if(m_formatContext)
        m_formatContext->pb = nullptr;
}

int AdtsInjector::reinitializeIoContext(int bufferSize)
{
    uninitializeIoContext();
    int result = initializeIoContext(bufferSize);
    if(result < 0)
        return result;

    m_formatContext->pb = m_ioContext;

    return result;
}
    
} // namespace usb_cam
} // namespace nx