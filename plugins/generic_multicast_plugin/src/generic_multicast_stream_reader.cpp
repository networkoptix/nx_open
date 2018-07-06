#include "generic_multicast_stream_reader.h"
#include "generic_multicast_media_packet.h"
#include "generic_multicast_io_device.h"
#include "utils.h"

#include <chrono>

#include <nx/network/socket_factory.h>
#include <nx/utils/std/cpp14.h>
#include <utils/media/nalUnits.h>
#include <utils/media/ffmpeg_helper.h>
#include <decoders/audio/aac.h>

namespace {

static const int kBufferSize = 32768 ;
static const int kWritableBufferFlag = 1;
static const int kReadOnlyBufferFlag = 0;

} // namespace


GenericMulticastStreamReader::GenericMulticastStreamReader(const QUrl& url):
    m_refManager(this),
    m_interrupted(false),
    m_url(url)
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
    close();
    m_interrupted = false;

    m_formatContext = avformat_alloc_context();
    if (!m_formatContext)
        return false;

    auto ioDevice = new GenericMulticastIoDevice(m_url);
    ioDevice->open(QIODevice::ReadOnly);

    auto buffer = (unsigned char*)av_malloc(kBufferSize);
    auto avioContext = avio_alloc_context(
        buffer,
        kBufferSize,
        kReadOnlyBufferFlag,
        ioDevice,
        readFromIoDevice,
        nullptr,
        nullptr);

    m_formatContext->pb = avioContext;
    m_formatContext->interrupt_callback.callback = checkIoDevice;
    m_formatContext->interrupt_callback.opaque = ioDevice;

    if (avformat_open_input(&m_formatContext, "", nullptr, nullptr) < 0)
    {
        QnFfmpegHelper::closeFfmpegIOContext(avioContext); //< m_formatContext became null here
        return false;
    }

    if (avformat_find_stream_info(m_formatContext, 0) < 0)
        return false;

    if (!initLayout())
        return false;

    return true;
}

void GenericMulticastStreamReader::close()
{
    if (m_formatContext)
    {
        QnFfmpegHelper::closeFfmpegIOContext(m_formatContext->pb);
        m_formatContext->pb = 0;
        avformat_close_input(&m_formatContext);
    }

    m_interrupted = true;
    m_formatContext = nullptr;
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
        mediaPacket->setCodecType(stream->codecpar->codec_id);
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
        AVStream* stream = m_formatContext->streams[i];
        AVCodecParameters* params = stream->codecpar;

        if (params->codec_type >= AVMEDIA_TYPE_NB)
            continue;

        if (stream->id && stream->id == lastStreamId)
            continue; // duplicate

        lastStreamId = stream->id;

        if (params->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            if (m_firstVideoIndex == -1)
                m_firstVideoIndex = i;

            m_streamIndexToChannelNumber[i] = ++videoChannelNumber;
        }
    }

    lastStreamId = -1;
    for (unsigned int i = 0; i < m_formatContext->nb_streams; i++)
    {
        AVStream* stream = m_formatContext->streams[i];
        AVCodecParameters* params = stream->codecpar;

        if (params->codec_type >= AVMEDIA_TYPE_NB)
            continue;

        if (stream->id && stream->id == lastStreamId)
            continue; // duplicate

        lastStreamId = stream->id;

        if (params->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            ++audioChannelNumber;
            m_streamIndexToChannelNumber[i] = videoChannelNumber + audioChannelNumber + 1;
            m_audioFormat.compressionType = fromFfmpegCodecIdToNx(params->codec_id);
            m_audioFormat.bitsPerCodedSample = params->bits_per_coded_sample;
            m_audioFormat.sampleRate = params->sample_rate;
            m_audioFormat.sampleFmt = fromFfmpegSampleFormatToNx((AVSampleFormat)params->format);
            m_audioFormat.bitrate = params->bit_rate;
            m_audioFormat.channelLayout = params->channel_layout;
            m_audioFormat.channels = params->channels;
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
    return packet.dts != AV_NOPTS_VALUE || packet.pts != AV_NOPTS_VALUE;
}

nxcip::UsecUTCTimestamp GenericMulticastStreamReader::packetTimestamp(const AVPacket& packet) const
{
    if (!isPacketStreamOk(packet))
        return nxcip::INVALID_TIMESTAMP_VALUE;

    const auto stream = m_formatContext->streams[packet.stream_index];
    const auto packetTime = packet.dts != AV_NOPTS_VALUE ? packet.dts : packet.pts;
    static const AVRational dstRate = {1, 1000000};
    return av_rescale_q(packetTime, stream->time_base, dstRate);
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

    if (packet.flags & AV_PKT_FLAG_KEY && stream->codecpar->codec_type != AVMEDIA_TYPE_AUDIO)
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

    if (stream->codecpar->codec_id == AV_CODEC_ID_AAC && hasAdtsHeader(packet))
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
