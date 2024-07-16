// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <decoders/audio/ffmpeg_audio_decoder.h>
#include <nx/media/ffmpeg/ffmpeg_utils.h>
#include <nx/utils/log/log.h>
#include <transcoding/ffmpeg_audio_transcoder.h>

extern "C" {
#include <libavformat/avformat.h>
} // extern "C"

namespace test {

class FfmpegMuxer
{
public:
    ~FfmpegMuxer()
    {
        if (m_formatContext)
            avformat_free_context(m_formatContext);
    }

    bool open(const std::string& container)
    {
        int status = avformat_alloc_output_context2(
            &m_formatContext, nullptr, container.c_str(), nullptr);
        if (status < 0)
        {
            NX_ERROR(this, "Failed to allocate format context: %1",
                nx::media::ffmpeg::avErrorToString(status));
            return false;
        }
        return true;
    }

    bool addStream(const AVCodecParameters* avCodecParams)
    {
        AVStream* stream = avformat_new_stream(m_formatContext, nullptr);
        if (!stream)
        {
            NX_ERROR(this, "Failed to allocate new stream");
            return false;
        }
        stream->id = m_formatContext->nb_streams - 1;

        int status = avcodec_parameters_copy(stream->codecpar, avCodecParams);
        if (status < 0)
        {
            NX_ERROR(this, "Failed to copy codec parameters: %1",
                nx::media::ffmpeg::avErrorToString(status));
            return false;
        }
        return true;
    }

private:
    AVFormatContext* m_formatContext = nullptr;
};

TEST(FfmpegMuxer, testTimeBase)
{
    auto testTimeBase =
        [](const char* container)
        {
            AVFormatContext* formatContext;
            ASSERT_EQ(avformat_alloc_output_context2(&formatContext, nullptr, container, nullptr), 0);
            ASSERT_EQ(avio_open(&formatContext->pb, "test", AVIO_FLAG_WRITE), 0);

            AVStream* stream = avformat_new_stream(formatContext, nullptr);
            ASSERT_TRUE(stream != 0);
            stream->id = formatContext->nb_streams - 1;
            stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
            stream->codecpar->codec_id = AV_CODEC_ID_MJPEG;
            stream->codecpar->width = 640;
            stream->codecpar->height = 480;

            ASSERT_EQ(avformat_write_header(formatContext, 0), 0);
            NX_DEBUG(NX_SCOPE_TAG, "Stream '%1' time base: {%2, %3}",
                container, stream->time_base.num, stream->time_base.den);

            ASSERT_EQ(av_write_trailer(formatContext), 0);
            avio_close(formatContext->pb);
            avformat_free_context(formatContext);
        };
    testTimeBase("matroska");
    testTimeBase("mov");
    testTimeBase("avi");
    testTimeBase("mp4");
}


TEST(FfmpegAudioDecoder, decodingG726)
{
    uint8_t data[] = {
        0xC3, 0x3B, 0x0C, 0x20, 0xFC, 0xFF, 0xEC, 0xF3, 0xF0, 0x3F, 0x3E, 0xCF, 0x33, 0xCF, 0xC3,
        0x0C, 0x23, 0x33, 0x8C, 0xD3, 0xB0, 0x0F, 0xF0, 0xF4, 0x0F, 0x00, 0x1F, 0xF3, 0x33, 0x00,
        0x00, 0x7C, 0x3E, 0x3C, 0xFF, 0x83, 0xFB, 0xFC, 0xFF, 0xF3, 0xE0, 0x30, 0x37, 0x03, 0xF1,
        0x00, 0x1C, 0x30, 0x1C, 0x30, 0x33, 0x3F, 0x37, 0xCF, 0xC3, 0x31, 0x33, 0xC0, 0x7F, 0x3C,
        0xF0, 0x40, 0xC3, 0x0F, 0x31, 0xC0, 0x03, 0x30, 0xEF, 0xCC, 0xCF, 0x03, 0x00, 0x3F, 0x0D,
        0x0F, 0x8F, 0x30, 0x10, 0x13, 0x00, 0x0C, 0x03, 0x3F, 0x88, 0x3F, 0xF8, 0xFF, 0xE3, 0xF2,
        0x3C, 0xFF, 0xCF, 0xE3, 0x3F, 0xCC, 0x07, 0x03, 0xFC, 0xC3, 0x0C, 0x01, 0x0C, 0x03, 0x20,
        0x30, 0xF7, 0xC3, 0xC0, 0x40, 0xCD, 0x00, 0xC0, 0x73, 0x33, 0x00, 0xCE, 0x0C, 0x30, 0xF0,
        0x33, 0x00, 0x0F, 0xFF, 0xFB, 0xCC, 0x0F, 0x40, 0x13, 0x33, 0xC0, 0x0C, 0x0F, 0x0C, 0x0C,
        0x30, 0xFF, 0xE0, 0xFE, 0xFF, 0xF3, 0x2F, 0x0F, 0x3F, 0xE3, 0xFF, 0xFD, 0xFC, 0xC3, 0xC3,
        0xFF, 0xFF, 0xC8, 0x00, 0x03, 0x00, 0x07, 0x3C, 0x1C, 0xCC, 0xF7, 0xCF, 0xC0, 0x01, 0xC0,
        0x03, 0x3C, 0x00, 0xC7, 0x0C, 0x3F, 0xFC, 0x09, 0xC3, 0x1C, 0xCC, 0xC0, 0xFF, 0x0C, 0xC1,
        0x07, 0xCC, 0x07, 0xCC, 0xF3, 0x30, 0xCB, 0xFC, 0xFF, 0x03, 0xCF, 0x8C, 0xFB, 0xF3, 0xFF,
        0x3F, 0x0F, 0x2C, 0xF8, 0x33};

    auto codecParameters = std::make_shared<CodecParameters>();
    const auto avCodecParams = codecParameters->getAvCodecParameters();
    avCodecParams->codec_type = AVMEDIA_TYPE_AUDIO;
    avCodecParams->codec_id = AV_CODEC_ID_ADPCM_G726;
    av_channel_layout_default(&avCodecParams->ch_layout, 1);
    avCodecParams->sample_rate = 8000;
    avCodecParams->format = AV_SAMPLE_FMT_S16;
    avCodecParams->bits_per_coded_sample = 2;

    QnWritableCompressedAudioDataPtr audioData(new QnWritableCompressedAudioData(sizeof(data)));
    audioData->compressionType = AV_CODEC_ID_ADPCM_G726;
    audioData->context = codecParameters;
    audioData->timestamp = 0;
    audioData->m_data.write((const char*)data, sizeof(data));

    // Media player decoder.
    {
        QnCompressedAudioDataPtr audioDataPtr = audioData;
        nx::utils::ByteArray result;
        QnFfmpegAudioDecoder decoder(audioDataPtr);
        ASSERT_TRUE(decoder.isInitialized());
        for (int i = 0; i < 10; ++i)
            ASSERT_TRUE(decoder.decode(audioDataPtr, result));
    }


    // Transcoder.
    {
        QnAbstractMediaDataPtr result;
        QnFfmpegAudioTranscoder transcoder(AV_CODEC_ID_MP2);
        ASSERT_TRUE(transcoder.open(audioData));
#if 0 // TODO: Fix G726 transcoding.
        for (int i = 0; i < 10; ++i)
            ASSERT_EQ(transcoder.transcodePacket(audioData, &result), 0);
#endif // 0
    }
}

} // namespace test
