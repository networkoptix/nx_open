// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ffmpeg_test_support.h"

#include <plugins/plugin_tools.h>
#include <nx/utils/test_support/utils.h>
#include <nx/streaming/video_data_packet.h>

namespace nx::utils::media::test_support {

namespace {

static void drawTimestampOnFrame(AVFrame* frame, uint64_t timestamp)
{
    NX_ASSERT(frame->linesize[0] >= kdrawPixelSize * 8);
    NX_ASSERT(frame->height >= kdrawPixelSize * 8);

    uint64_t bitMask = 1;
    for (int bitNum = 0; bitNum < 64; ++bitNum)
    {
        uint8_t color = (timestamp & bitMask) ? 0xff : 0x00;
        int posX = (bitNum % 8) * kdrawPixelSize;
        int posY = (bitNum / 8) * kdrawPixelSize;
        uint8_t* data = frame->data[0] + posY * frame->linesize[0] + posX;
        for (int y = 0; y < kdrawPixelSize; ++y)
        {
            for (int x = 0; x < kdrawPixelSize; ++x)
                data[y * frame->linesize[0] + x] = color;
        }
        bitMask <<= 1;
    }
}

class MediaPacket: public nxcip::VideoDataPacket
{
public:
    MediaPacket(
        const AVPacket* packet,
        std::chrono::microseconds timestamp,
        nxcip::CompressionType codecId) noexcept(false)
        :
        m_refManager(this),
        m_data(packet->data),
        m_size(packet->size),
        m_timestampUs(timestamp.count()),
        m_streamIndex(packet->stream_index),
        m_codecId(codecId)
    {
        if (packet->flags | AV_PKT_FLAG_KEY)
            m_flags |= nxcip::MediaDataPacket::fKeyPacket;
    }

    virtual ~MediaPacket() override
    {
        free(m_data);
    }

    virtual nxcip::UsecUTCTimestamp timestamp() const override
    {
        return m_timestampUs;
    }

    virtual nxcip::DataPacketType type() const override
    {
        // #TODO #akulikov Use real value instead of the dummy constant.
        return nxcip::dptVideo;
    }

    virtual const void* data() const override
    {
        return m_data;
    }

    virtual unsigned int dataSize() const override
    {
        return  m_size;
    }

    virtual unsigned int channelNumber() const override
    {
        return m_streamIndex;
    }

    virtual nxcip::CompressionType codecType() const override
    {
        return m_codecId;
    }

    virtual unsigned int flags() const override
    {
        return m_flags;
    }

    virtual unsigned int cSeq() const override
    {
        return 0;
    }

    virtual int addRef() const override { return m_refManager.addRef(); }
    virtual int releaseRef() const override { return m_refManager.releaseRef(); }
    virtual void* queryInterface(const nxpl::NX_GUID& interfaceID) override
    {
        if (memcmp(
                interfaceID.bytes,
                nxcip::IID_VideoDataPacket.bytes,
                sizeof(interfaceID.bytes)) == 0
            || memcmp(
                   interfaceID.bytes,
                   nxcip::IID_MediaDataPacket.bytes,
                   sizeof(interfaceID.bytes)) == 0)
        {
            addRef();
            return this;
        }

        return nullptr;
    }

    virtual nxcip::Picture* getMotionData() const override { return nullptr; }

private:
    nxpt::CommonRefManager m_refManager;
    void* m_data;
    const int m_size;
    const int64_t m_timestampUs;
    const int m_streamIndex;
    const nxcip::CompressionType m_codecId;
    int m_flags = 0;
};

} // namespace

uint64_t getTimestampFromFrame(const AVFrame* frame)
{
    NX_ASSERT(frame->linesize[0] >= kdrawPixelSize * 8);
    NX_ASSERT(frame->height >= kdrawPixelSize * 8);

    uint64_t result = 0;
    uint64_t bitMask = 1;
    for (int bitNum = 0; bitNum < 64; ++bitNum)
    {
        int posX = (bitNum % 8) * kdrawPixelSize;
        int posY = (bitNum / 8) * kdrawPixelSize;
        uint8_t* data = frame->data[0] + posY * frame->linesize[0] + posX;
        int color = 0;
        for (int y = 0; y < kdrawPixelSize; ++y)
        {
            for (int x = 0; x < kdrawPixelSize; ++x)
                color += data[y * frame->linesize[0] + x];
        }
        float averageColor = color / static_cast<float>(kdrawPixelSize * kdrawPixelSize);
        if (averageColor >= 128.0f)
            result |= bitMask;

        bitMask <<= 1;
    }

    return result;
}

AVPacketWithTimestampGenerator::AVPacketWithTimestampGenerator(
    AVCodecID codecId, int width, int height)
{

    auto encoder = avcodec_find_encoder(codecId);
    if (!encoder)
        throw std::runtime_error("avcodec_find_encoder() failed");

    m_codecContext = avcodec_alloc_context3(encoder);
    if (!m_codecContext)
        throw std::runtime_error("Could not allocate an encoding context");

    m_codecContext->bit_rate = 400000;
    m_codecContext->width = width;
    m_codecContext->height = height;
    m_codecContext->time_base = {1, 25};
    m_codecContext->framerate = {25, 1};
    m_codecContext->gop_size = 25;
    m_codecContext->pix_fmt = AV_PIX_FMT_YUV420P;
    m_codecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (avcodec_open2(m_codecContext, encoder, nullptr) < 0)
        throw std::runtime_error("avcodec_open2() failed");

    if (!(m_packet = av_packet_alloc()))
        throw std::runtime_error("av_packet_alloc() failed");

    m_frame = av_frame_alloc();
    if (!m_frame)
        throw std::runtime_error("av_frame_alloc() failed");

    m_frame->format = m_codecContext->pix_fmt;
    m_frame->width = width;
    m_frame->height = height;
    m_frame->pts = 0;
    if (av_frame_get_buffer(m_frame, 64) < 0)
        throw std::runtime_error("av_frame_get_buffer() failed");

    if (av_frame_make_writable(m_frame) < 0)
        throw std::runtime_error("av_frame_make_writable() failed");
}

AVPacket* AVPacketWithTimestampGenerator::next(AVRational streamTimeBase)
{
    for (size_t i = 0; i < 8; i++)
    {
        if (m_frame->buf[i])
            memset(m_frame->buf[i]->data, 0, static_cast<size_t>(m_frame->buf[i]->size));
    }

    const int64_t timestampMs = av_rescale_q(m_frame->pts, m_codecContext->time_base, {1, 1000});
    drawTimestampOnFrame(m_frame, timestampMs);
    NX_ASSERT(timestampMs == (int64_t)getTimestampFromFrame(m_frame));

    NX_ASSERT(avcodec_send_frame(m_codecContext, m_frame) == 0);
    NX_ASSERT(avcodec_receive_packet(m_codecContext, m_packet) == 0);

    if (m_codecContext->codec_id == AV_CODEC_ID_H264)
    {
        NX_ASSERT(
            m_packet->data[0] == 0
            && m_packet->data[1] == 0
            && m_packet->data[2] == 0
            && m_packet->data[3] == 1);
    }

    m_packet->pts = m_packet->dts = av_rescale_q(m_frame->pts, m_codecContext->time_base, streamTimeBase);
    m_frame->pts += 1;

    return m_packet;
}

AVPacketWithTimestampGenerator::~AVPacketWithTimestampGenerator()
{
    av_frame_free(&m_frame);
    av_packet_free(&m_packet);
    avcodec_free_context(&m_codecContext);
}

TimeStampedDataProvider::TimeStampedDataProvider(
    const QnResourcePtr& resource,
    int maxPacketCount)
    :
    QnAbstractMediaStreamDataProvider(resource),
    m_generator(kCodecId, kWidth, kHeight),
    m_maxPacketCount(maxPacketCount)
{
}

TimeStampedDataProvider::~TimeStampedDataProvider()
{
    stop();
}

void TimeStampedDataProvider::run()
{
    auto codecParameters = std::make_shared<CodecParameters>(m_generator.getAvCodecContext());

    while (m_packetCount++ < m_maxPacketCount)
    {
        AVPacket* avPacket = m_generator.next({1, 1000'000});
        QnAbstractMediaDataPtr mediaData;
        auto videoFrame = new QnWritableCompressedVideoData();

        if (avPacket->flags & AV_PKT_FLAG_KEY)
            videoFrame->flags |= QnAbstractMediaData::MediaFlags_AVKey;

        videoFrame->channelNumber = 0;
        videoFrame->timestamp = avPacket->dts;
        videoFrame->compressionType = kCodecId;
        videoFrame->width = kWidth;
        videoFrame->height = kHeight;
        videoFrame->context = codecParameters;
        QnByteArray data(CL_MEDIA_ALIGNMENT, 0, AV_INPUT_BUFFER_PADDING_SIZE);
        data.write((const char*) avPacket->data, avPacket->size);
        videoFrame->m_data = data;
        mediaData.reset(videoFrame);

        if (m_packetCount >= m_actionPacketNumber
            && (avPacket->flags & AV_PKT_FLAG_KEY)
            && m_action)
        {
            m_action();
        }

        putData(mediaData);
    }
}

void TimeStampedDataProvider::setAction(std::function<void()> action, int packetNumber)
{
    m_action = action;
    m_actionPacketNumber = packetNumber;
}

} // nx::utils::media::test_support
