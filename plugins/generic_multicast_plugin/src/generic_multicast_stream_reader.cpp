#include "generic_multicast_stream_reader.h"
#include "generic_multicast_media_packet.h"
#include "generic_multicast_io_device.h"

#include <chrono>

#include <nx/network/socket_factory.h>
#include <utils/media/nalUnits.h> 
#include <utils/media/ffmpeg_helper.h>
#include <decoders/audio/aac.h>

namespace {

static const int kBufferSize = 32768 ;
static const int kWritableBufferFlag = 1;
static const int kReadOnlyBufferFlag = 0;

extern "C" {

int readFromQIODevice(void* opaque, uint8_t* buf, int size)
{

    auto ioDevice = reinterpret_cast<QIODevice*>(opaque);
    auto result = ioDevice->read((char*)buf, size);

    return result;
}

} // extern "C"

nxcip::AudioFormat::SampleType fromFfmpegSampleFormatToNx(AVSampleFormat avFormat)
{
    // TODO: #dmishin maybe it's worth to extend nxcip::SampleType enum
    switch (avFormat)
    {
        case AV_SAMPLE_FMT_U8:
        case AV_SAMPLE_FMT_U8P:
            return nxcip::AudioFormat::SampleType::stU8;
        case AV_SAMPLE_FMT_S16:
        case AV_SAMPLE_FMT_S16P:
            return nxcip::AudioFormat::SampleType::stS16;
        case AV_SAMPLE_FMT_S32:
        case AV_SAMPLE_FMT_S32P:
            return nxcip::AudioFormat::SampleType::stS32;
        case AV_SAMPLE_FMT_FLT:
        case AV_SAMPLE_FMT_FLTP:
            return nxcip::AudioFormat::SampleType::stFLT;
        default:
            return nxcip::AudioFormat::SampleType::stU8; 
    }
};

nxcip::CompressionType fromFfmpegCodecIdToNx(AVCodecID codecId)
{
    switch (codecId)
    {
        case AV_CODEC_ID_H264:
            return nxcip::CompressionType::AV_CODEC_ID_H264;
        case AV_CODEC_ID_MPEG2VIDEO:
            return nxcip::CompressionType::AV_CODEC_ID_MPEG2VIDEO;
        case AV_CODEC_ID_H263:
            return nxcip::CompressionType::AV_CODEC_ID_H263;
        case AV_CODEC_ID_MJPEG:
            return nxcip::CompressionType::AV_CODEC_ID_MJPEG;
        case AV_CODEC_ID_MPEG4:
            return nxcip::CompressionType::AV_CODEC_ID_MPEG4;
        case AV_CODEC_ID_THEORA:
            return nxcip::CompressionType::AV_CODEC_ID_THEORA;
        case AV_CODEC_ID_PNG:
            return nxcip::CompressionType::AV_CODEC_ID_PNG;
        case AV_CODEC_ID_GIF:
            return nxcip::CompressionType::AV_CODEC_ID_GIF;
        case AV_CODEC_ID_MP2:
            return nxcip::CompressionType::AV_CODEC_ID_MP2;
        case AV_CODEC_ID_MP3:
            return nxcip::CompressionType::AV_CODEC_ID_MP3;
        case AV_CODEC_ID_AAC:
            return nxcip::CompressionType::AV_CODEC_ID_AAC;
        case AV_CODEC_ID_AC3:
            return nxcip::CompressionType::AV_CODEC_ID_AC3;
        case AV_CODEC_ID_DTS:
            return nxcip::CompressionType::AV_CODEC_ID_DTS;
        case AV_CODEC_ID_PCM_S16LE:
            return nxcip::CompressionType::AV_CODEC_ID_PCM_S16LE;
        case AV_CODEC_ID_PCM_MULAW:
            return nxcip::CompressionType::AV_CODEC_ID_PCM_MULAW;
        case AV_CODEC_ID_VORBIS:
            return nxcip::CompressionType::AV_CODEC_ID_VORBIS;
        default:
            return nxcip::CompressionType::AV_CODEC_ID_NONE;
    }
}

} // namespace 


GenericMulticastStreamReader::GenericMulticastStreamReader(const QUrl& url):
    m_refManager(this),
    m_interrupted(false),
    m_url(url),
    m_ioDevice(new GenericMulticastIoDevice(url))
{
}

GenericMulticastStreamReader::~GenericMulticastStreamReader()
{   
    close();
}

void* GenericMulticastStreamReader::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (memcmp(&interfaceId, &nxcip::IID_StreamReader, sizeof(nxpl::NX_GUID)) == 0)
    {
        addRef();
        return static_cast<nxcip::StreamReader*>(this);
    }
    if (memcmp(&interfaceId, &nxpl::IID_PluginInterface, sizeof(nxpl::NX_GUID)) == 0)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return NULL;
}

unsigned int GenericMulticastStreamReader::addRef()
{
    return m_refManager.addRef();
}

unsigned int GenericMulticastStreamReader::releaseRef()
{
    return m_refManager.releaseRef();
}

bool GenericMulticastStreamReader::open()
{
    m_interrupted = false;

    if (!m_ioDevice->isOpen())
        m_ioDevice->open(QIODevice::ReadOnly);

    auto buffer = (unsigned char*)av_malloc(kBufferSize);
    auto avioContext = avio_alloc_context(
        buffer,
        kBufferSize,
        kReadOnlyBufferFlag,
        m_ioDevice.get(),
        readFromQIODevice,
        nullptr,
        nullptr);

    m_formatContext = avformat_alloc_context();

    if (!m_formatContext)
        return false;

    m_formatContext->pb = avioContext;

    if (avformat_open_input(&m_formatContext, "", nullptr, nullptr) < 0)
        return false;

    if (avformat_find_stream_info(m_formatContext, 0) < 0)
        return false;

    if (!initLayout())
        return false;

    m_startTimeUs = currentTimeSinceEpochUs();
    return true;
}

void GenericMulticastStreamReader::close()
{
    m_interrupted = true;
    avformat_close_input(&m_formatContext);
    m_formatContext = nullptr;

    if (m_avioContext)
    {
        av_freep(&m_avioContext->buffer);
        av_freep(&m_avioContext);
    }

    m_ioDevice->close();
}

int GenericMulticastStreamReader::getNextData(nxcip::MediaDataPacket** outPacket)
{
    if (m_interrupted)
        return nxcip::NX_INTERRUPTED;

    AVStream* stream = nullptr;
    AVPacket packet;
    
    while (true)
    {
        if (m_interrupted)
            return nxcip::NX_INTERRUPTED;

        if (av_read_frame(m_formatContext, &packet) < 0)
            return nxcip::NX_IO_ERROR;

        if (!isPacketOk(packet))
        {
            av_packet_unref(&packet);
            continue;
        }

        uint8_t* dataPointer = packet.data;
        int dataSize = packet.size;

        Extras extras;
        if (!preprocessPacket(packet, &dataPointer, &dataSize, &extras))
            continue;        

        stream = m_formatContext->streams[packet.stream_index];
        auto mediaPacket = std::make_unique<GenericMulticastMediaPacket>(&m_allocator);
        mediaPacket->setCodecType(stream->codec->codec_id);
        mediaPacket->setTimestamp(packetTimestamp(packet));
        mediaPacket->setChannelNumber(packetChannelNumber(packet));
        mediaPacket->setFlags(packetFlags(packet));
        mediaPacket->setType(packetDataType(packet));
        mediaPacket->setData(dataPointer, dataSize);
        mediaPacket->setExtradata(extras.extradata);

        *outPacket = mediaPacket.get();
        mediaPacket.release();

        av_packet_unref(&packet);
        return nxcip::NX_NO_ERROR;
    }    
}

void GenericMulticastStreamReader::interrupt()
{
    m_interrupted = true;
}

bool GenericMulticastStreamReader::initLayout()
{
    resetAudioFormat();

    m_firstVideoIndex = -1;
    int lastStreamId = -1;
    int videoChannelNumber = -1;
    int audioChannelNumber = -1;

    for (unsigned int i = 0; i < m_formatContext->nb_streams; i++)
    {
        AVStream *stream = m_formatContext->streams[i];
        AVCodecContext *codecContext = stream->codec;

        if (codecContext->codec_type >= AVMEDIA_TYPE_NB)
            continue;

        if (stream->id && stream->id == lastStreamId)
            continue; // duplicate

        lastStreamId = stream->id;

        if (codecContext->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            if (m_firstVideoIndex == -1)
                m_firstVideoIndex = i;

            m_streamIndexToChannelNumber[i] = ++videoChannelNumber;
        }
    }

    lastStreamId = -1;
    for (unsigned int i = 0; i < m_formatContext->nb_streams; i++)
    {
        AVStream *stream = m_formatContext->streams[i];
        AVCodecContext *codecContext = stream->codec;

        if (codecContext->codec_type >= AVMEDIA_TYPE_NB)
            continue;

        if (stream->id && stream->id == lastStreamId)
            continue; // duplicate

        lastStreamId = stream->id;

        if (codecContext->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            ++audioChannelNumber;
            m_streamIndexToChannelNumber[i] = videoChannelNumber + audioChannelNumber + 1;
            m_audioFormat.compressionType = fromFfmpegCodecIdToNx(codecContext->codec_id);
            m_audioFormat.bitsPerCodedSample = codecContext->bits_per_coded_sample;
            m_audioFormat.sampleRate = codecContext->sample_rate;
            m_audioFormat.sampleFmt = fromFfmpegSampleFormatToNx(codecContext->sample_fmt);
            m_audioFormat.bitrate = codecContext->bit_rate;
            m_audioFormat.channelLayout = AV_CH_LAYOUT_STEREO;
            m_audioFormat.channels = 2;
        }
    }

    return true;
}

bool GenericMulticastStreamReader::isPacketOk(const AVPacket& packet) const
{
    if (packet.size <= 0)
        return false;

    if (!isPacketDataTypeOk(packet))
        return false;

    if (!isPacketTimestampOk(packet))
        return false;

    return true;
}

bool GenericMulticastStreamReader::isPacketStreamOk(const AVPacket& packet) const
{
    if (packet.stream_index < 0)
        return false;

    unsigned int streamIndex = packet.stream_index;

    bool noCorrespondentChannel = m_streamIndexToChannelNumber.find(streamIndex)
        == m_streamIndexToChannelNumber.end();

    if (noCorrespondentChannel)
        return false;

    NX_ASSERT(m_formatContext, lm("No AVFormatContext exists for provided AVPacket"));
    if (!m_formatContext)
        return false;

    if (m_formatContext->nb_streams <= streamIndex)
        return false;

    if (!m_formatContext->streams[streamIndex])
        return false;

    return true;
}

bool GenericMulticastStreamReader::isPacketDataTypeOk(const AVPacket& packet) const
{
    if (!isPacketStreamOk(packet))
        return false;

    auto stream = m_formatContext->streams[packet.stream_index];

    return (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && m_audioEnabled)
        || stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO;
}


bool GenericMulticastStreamReader::isPacketTimestampOk(const AVPacket& packet) const
{
    if(!isPacketStreamOk(packet))
        return false;

    return packetTimestamp(packet) != nxcip::INVALID_TIMESTAMP_VALUE;
}

nxcip::UsecUTCTimestamp GenericMulticastStreamReader::packetTimestamp(const AVPacket& packet) const
{
    if (!isPacketStreamOk(packet))
        return nxcip::INVALID_TIMESTAMP_VALUE;

    auto stream = m_formatContext->streams[packet.stream_index];
    double timeBase = av_q2d(stream->time_base) * 1000000;

    auto firstDts = m_firstVideoIndex >= 0 
        ? m_formatContext->streams[packet.stream_index]->first_dts
        : 0;

    if (firstDts == qint64(AV_NOPTS_VALUE))
        firstDts = 0;

    auto packetTime = packet.dts != AV_NOPTS_VALUE ? packet.dts : packet.pts;

    if (packetTime == AV_NOPTS_VALUE)
        return nxcip::INVALID_TIMESTAMP_VALUE;
    
    return std::max<>(
        (nxcip::UsecUTCTimestamp)0, 
        (nxcip::UsecUTCTimestamp)(timeBase * (packetTime - firstDts)))
        + m_startTimeUs;
}

unsigned int GenericMulticastStreamReader::packetChannelNumber(const AVPacket& packet) const
{
    auto channelNumber = m_streamIndexToChannelNumber.find(packet.stream_index);

    NX_ASSERT(
        channelNumber != m_streamIndexToChannelNumber.end(),
        lm("No correspondent channel for stream"));

    return channelNumber->second;
}

unsigned int GenericMulticastStreamReader::packetFlags(const AVPacket& packet) const
{
    if (!isPacketStreamOk(packet))
        return 0;

    unsigned int flags = 0;
    
    auto stream = m_formatContext->streams[packet.stream_index];

    if (packet.flags & AV_PKT_FLAG_KEY && stream->codec->codec_type != AVMEDIA_TYPE_AUDIO)
        flags |= nxcip::MediaDataPacket::Flags::fKeyPacket;

    return flags;
}

nxcip::DataPacketType GenericMulticastStreamReader::packetDataType(const AVPacket& packet) const
{
    if (!isPacketStreamOk(packet))
        return nxcip::DataPacketType::dptEmpty; //< TODO: #dmishin handle this error properly

    auto stream = m_formatContext->streams[packet.stream_index];
    auto type = stream->codecpar->codec_type;

    switch (type)
    {
        case AVMEDIA_TYPE_AUDIO:
            return nxcip::DataPacketType::dptAudio;
        case AVMEDIA_TYPE_VIDEO:
            return nxcip::DataPacketType::dptVideo;
        default:
            return nxcip::DataPacketType::dptEmpty; //< And also this
    }
}

nxcip::AudioFormat GenericMulticastStreamReader::audioFormat() const
{
    return m_audioFormat;
}

void GenericMulticastStreamReader::resetAudioFormat()
{
    m_audioFormat = nxcip::AudioFormat();
}

bool GenericMulticastStreamReader::preprocessPacket(
    const AVPacket& packet,
    uint8_t** data,
    int* dataSize,
    Extras* outExtras)
{
    if (!isPacketStreamOk(packet))
        return false;

    auto stream = m_formatContext->streams[packet.stream_index];

    if (stream->codec->codec_id == AV_CODEC_ID_AAC && hasAdtsHeader(packet))
        removeAdtsHeaderAndFillExtradata(packet, data, dataSize, outExtras);

    return true;
}

bool GenericMulticastStreamReader::hasAdtsHeader(const AVPacket& packet)
{
    if (packet.size < 2)
        return false;

    return packet.data[0] == 0xff && packet.data[1] >= 0xf0;
}

bool GenericMulticastStreamReader::removeAdtsHeaderAndFillExtradata(
    const AVPacket& packet,
    uint8_t** dataPointer,
    int* dataSize,
    Extras* outExtras)
{
    if (!isPacketStreamOk(packet))
        return false;

    AdtsHeader header;
    if (!header.decodeFromFrame(packet.data, packet.size))
        return false;

    QByteArray extradata;
    if (!header.encodeToFfmpegExtradata(&outExtras->extradata))
        return false;

    *dataPointer += header.length();
    *dataSize -= header.length();

    return true;
}

int64_t GenericMulticastStreamReader::currentTimeSinceEpochUs() const
{
    namespace chr = std::chrono;

    auto now = chr::system_clock::now();
    auto now_us = chr::time_point_cast<chr::microseconds>(now);

    return now_us.time_since_epoch().count();
}

void GenericMulticastStreamReader::setAudioEnabled(bool audioEnabled)
{
    m_audioEnabled = audioEnabled;
}

