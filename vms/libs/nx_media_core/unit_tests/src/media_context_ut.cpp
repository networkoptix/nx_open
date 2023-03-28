// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/media/codec_parameters.h>
#include <nx/media/ffmpeg_helper.h>
#include <nx/utils/log/assert.h>

/**
 * Fill with dummy data the fields of AVCodecParameters which are present in CodecParameters.
 */
void fillDummyAvCodecParameters(AVCodecParameters* avCodecParams)
{
    static const char* const kExtradataDummy = "extradata dummy bytes (non-zero-terminated)";

    avCodecParams->codec_id = AV_CODEC_ID_FFMETADATA;
    avCodecParams->codec_type = AVMEDIA_TYPE_VIDEO;
    avCodecParams->extradata_size = (int) strlen(kExtradataDummy);
    QnFfmpegHelper::copyAvCodecContextField((void**) &avCodecParams->extradata, kExtradataDummy,
        strlen(kExtradataDummy));

    avCodecParams->channels = 113;
    avCodecParams->sample_rate = 114;
    avCodecParams->format = AV_SAMPLE_FMT_NONE;
    avCodecParams->bits_per_coded_sample = 115;
    avCodecParams->width = 118;
    avCodecParams->height = 119;
    avCodecParams->bit_rate = 120;
    avCodecParams->channel_layout = ((quint64) INT_MAX) * 10;
    avCodecParams->block_align = -10;
}

/**
 * Check that the memory regions are independently allocated and have equal contents.
 * Pointers are allowed to be null only if size is 0.
 */
void expectMemsAreDeepCopies(
    const void* expected, const void* actual, int size, const char *fieldName)
{
    const QByteArray captionString = ("Mem field: " + QString::fromLatin1(fieldName)).toLatin1();
    const char* const caption = captionString.constData();

    if (size == 0)
    {
        if (expected == nullptr || actual == nullptr)
        {
            EXPECT_TRUE(expected == nullptr) << caption;
            EXPECT_TRUE(actual == nullptr) << caption;
        }
    }
    else
    {
        ASSERT_TRUE(expected != nullptr) << caption;
        ASSERT_TRUE(actual != nullptr) << caption;
        EXPECT_NE(expected, actual) << caption;
        EXPECT_TRUE(memcmp(expected, actual, size) == 0) << caption;
    }
}

void testMediaContextFieldsEqual(
    const CodecParameters& expected, const CodecParameters& actual)
{
    EXPECT_EQ(expected.getCodecId(), actual.getCodecId());
    EXPECT_EQ(expected.getCodecType(), actual.getCodecType());

    ASSERT_EQ(expected.getExtradataSize(), actual.getExtradataSize());
    expectMemsAreDeepCopies(expected.getExtradata(), actual.getExtradata(),
        expected.getExtradataSize(), "extradata");

    EXPECT_EQ(expected.getChannels(), actual.getChannels());
    EXPECT_EQ(expected.getSampleRate(), actual.getSampleRate());
    EXPECT_EQ(expected.getSampleFmt(), actual.getSampleFmt());
    EXPECT_EQ(expected.getBitsPerCodedSample(), actual.getBitsPerCodedSample());
    EXPECT_EQ(expected.getWidth(), actual.getWidth());
    EXPECT_EQ(expected.getHeight(), actual.getHeight());
    EXPECT_EQ(expected.getBitRate(), actual.getBitRate());
    EXPECT_EQ(expected.getChannelLayout(), actual.getChannelLayout());
    EXPECT_EQ(expected.getBlockAlign(), actual.getBlockAlign());
}

TEST(QnMediaDataPacket, serialization)
{
    CodecParameters avCodecMediaContext;
    fillDummyAvCodecParameters(avCodecMediaContext.getAvCodecParameters());

    const QByteArray data = avCodecMediaContext.serialize();
    ASSERT_TRUE(data.length() > 0);

    CodecParameters avCodecMediaContextParsed;
    ASSERT_EQ(avCodecMediaContextParsed.deserialize(data.data(), data.size()), true);

    testMediaContextFieldsEqual(
        avCodecMediaContext, avCodecMediaContextParsed);
}
