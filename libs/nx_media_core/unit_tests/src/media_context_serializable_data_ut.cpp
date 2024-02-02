// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/fusion/model_functions.h>
#include <nx/kit/debug.h>
#include <nx/media/av_codec_helper.h>
#include <nx/media/media_context_serializable_data.h>

namespace nx::streaming::test {

TEST(MediaContextSerializableData, main)
{
    QnMediaContextSerializableData data;
    data.codecId = AV_CODEC_ID_H264;
    data.codecType = AVMEDIA_TYPE_VIDEO;
    data.sampleFmt = AV_SAMPLE_FMT_U8;
    data.bitsPerCodedSample = 8;
    data.width = 1920;
    data.height = 1080;
    data.bitRate = 80000000;
    data.channelLayout = 42;
    data.blockAlign = 32;

    static const std::vector<uint8_t> expectedData{
        0x5B,0x6C,0x00,0x00,0x00,0x1B,0x6C,0x00,0x00,0x00,0x00,0x5B,0x24,0x55,0x23,0x55,
        0x00,0x6C,0x00,0x00,0x00,0x00,0x6C,0x00,0x00,0x00,0x00,0x6C,0x00,0x00,0x00,0x00,
        0x6C,0x00,0x00,0x00,0x08,0x6C,0x00,0x00,0x07,0x80,0x6C,0x00,0x00,0x04,0x38,0x6C,
        0x04,0xC4,0xB4,0x00,0x4C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x2A,0x6C,0x00,0x00,
        0x00,0x20,0x5D
    };

    const QByteArray serializedData = data.serialize();

    EXPECT_EQ(serializedData, QByteArray((const char*) expectedData.data(), expectedData.size()));

    if (HasFailure())
    {
        NX_PRINT_HEX_DUMP("Expected", expectedData.data(), expectedData.size());
        NX_PRINT_HEX_DUMP("Actual", serializedData.constData(), serializedData.size());
    }

    QnMediaContextSerializableData deserialized;
    deserialized.deserialize(serializedData);

    ASSERT_EQ(data.codecId, deserialized.codecId);
    ASSERT_EQ(data.codecType, deserialized.codecType);
    ASSERT_EQ(data.sampleFmt, deserialized.sampleFmt);
    ASSERT_EQ(data.bitsPerCodedSample, deserialized.bitsPerCodedSample);
    ASSERT_EQ(data.width, deserialized.width);
    ASSERT_EQ(data.height, deserialized.height);
    ASSERT_EQ(data.bitRate, deserialized.bitRate);
    ASSERT_EQ(data.channelLayout, deserialized.channelLayout);
    ASSERT_EQ(data.blockAlign, deserialized.blockAlign);
}

} // namespace nx::streaming::test
