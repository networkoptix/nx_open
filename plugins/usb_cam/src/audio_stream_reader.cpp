#include "audio_stream_reader.h"

#include <plugins/plugin_container_api.h>

namespace nx {
namespace usb_cam {

namespace {

int constexpr RETRY_LIMIT = 10;

const char * deviceType()
{
#ifdef _WIN32
    return "dshow";
#elif __linux__
    return "alsa";
#endif
}

} // namespace

AudioStreamReader::AudioStreamReader(const std::string& url, nxpl::TimeProvider * timeProvider) :
    m_url(url),
    m_timeProvider(timeProvider),
    m_initialized(false),
    m_initCode(0),
    m_retries(0)
{
}

AudioStreamReader::~AudioStreamReader()
{
    uninitialize();
}

int AudioStreamReader::getNextData(ffmpeg::Packet * packet)
{
    if (m_retries >= RETRY_LIMIT)
        return m_initCode;

    if (!ensureInitialized())
    {
        ++m_retries;
        return m_initCode;
    }

    ffmpeg::Frame frame;
    int result = decodeNextFrame(frame.frame());
    if(result < 0)
        return result;

    result = encode(packet->packet(), frame.frame());
    if (result < 0)
        return result;

    packet->setTimeStamp(m_timeProvider->millisSinceEpoch());
    
    return 0;
}

int AudioStreamReader::initialize()
{
    m_initCode = 0;
    m_initCode = initializeInputFormat();
    if (m_initCode < 0)
        return m_initCode;

    m_initCode = initializeDecoder();
    if (m_initCode < 0)
        return m_initCode;

    m_initCode = initializeEncoder();
    if (m_initCode < 0)
        return m_initCode;

    m_initialized = true;
    return 0;
}

void AudioStreamReader::uninitialize()
{
    if (m_decoder)
        m_decoder->flush();
    m_decoder.reset(nullptr);
    if (m_encoder)
        m_encoder->flush();
    m_encoder.reset(nullptr);
    m_inputFormat.reset(nullptr);
    m_initialized = false;
}

bool AudioStreamReader::ensureInitialized()
{
    if(!m_initialized)
        initialize();
    return m_initialized;
}

int AudioStreamReader::initializeInputFormat()
{
    auto inputFormat = std::make_unique<ffmpeg::InputFormat>();
    int initCode = inputFormat->initialize(deviceType());
    if (initCode < 0)
        return initCode;
    
    int openCode = inputFormat->open(m_url.c_str());
    if (openCode < 0)
        return openCode;

    m_inputFormat = std::move(inputFormat);
    return 0;
}

int AudioStreamReader::initializeDecoder()
{
    auto decoder = std::make_unique<ffmpeg::Codec>();
    AVStream * stream = m_inputFormat->findStream(AVMEDIA_TYPE_AUDIO);
    if (!stream)
        return AVERROR_DECODER_NOT_FOUND;

    int initCode = decoder->initializeDecoder(stream->codecpar);
    if (initCode < 0)
        return initCode;


    int openCode = decoder->open();
    if (openCode < 0)
        return openCode;

    m_decoder = std::move(decoder);
    return 0;
}

int AudioStreamReader::initializeEncoder()
{
    auto encoder = std::make_unique<ffmpeg::Codec>();
    int initCode = encoder->initializeEncoder(AV_CODEC_ID_MP3);
    if (initCode < 0)
        return initCode;

    int openCode = encoder->open();
    if (openCode < 0)
        return openCode;

    m_encoder = std::move(encoder);
    return 0;
}

int AudioStreamReader::decodeNextFrame(AVFrame * outFrame) const
{
    int result = 0;
    bool gotFrame = false;
    bool needSend = true;
    bool needReceive = true;
    while (!gotFrame)
    {
        while (needSend)
        {
            ffmpeg::Packet packet(m_inputFormat->audioCodecID());
            result = m_inputFormat->readFrame(packet.packet());
            if (result < 0)
                return result;
            
            result = m_decoder->sendPacket(packet.packet());
            if (result == AVERROR(EAGAIN)) // need to decode previous packets
                needSend = false;
            else if(result < 0) // something very bad happened
                return result;
        }

        while (needReceive)
        {
            result = m_decoder->receiveFrame(outFrame);
            if (result == 0) // got a frame
            {
                gotFrame = true;
                needReceive = false;
            }
            else if (result == AVERROR(EAGAIN)) // need to send more input to decoder
            {
                needSend = true;
                needReceive = false;
            }
            else // something very bad happened
            {
                return result;
            }
        }
    }
    return result;
}

int AudioStreamReader::encode(AVPacket * outPacket, const AVFrame * frame) const
{
    int result = 0;
    bool send = true;
    result = m_encoder->sendFrame(frame);
    if (result != 0 && result == AVERROR(EAGAIN)) // ready to read packets
        return result;

    result = m_encoder->receivePacket(outPacket);
    if (result == AVERROR(EAGAIN))
        result = 0;
    
    return result;
}

} //namespace usb_cam
} //namespace nx