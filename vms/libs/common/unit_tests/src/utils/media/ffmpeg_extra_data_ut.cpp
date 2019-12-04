#include <gtest/gtest.h>

#include <utils/media/h264_utils.h>

using namespace nx::media_utils;

TEST(media_utils, extradata_build_h264)
{
    uint8_t frameData[] = {
        0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0x00, 0x29, 0xe3, 0x50, 0x14, 0x07, 0xb6, 0x02, 0xdc,
        0x04, 0x04, 0x06, 0x90, 0x78, 0x91, 0x15, 0x00, 0x00, 0x00, 0x01, 0x68, 0xce, 0x3c, 0x80,
        0x00, 0x00, 0x00, 0x01, 0x65, 0x88, 0x84, 0x00, 0x00, 0x0c, 0x21, 0x18, 0xa0, 0x00, 0x67,
        0xf9, 0x39, 0x39, 0x39, 0x39, 0x38, 0x8f, 0xd1, 0x1e, 0xa4, 0xe2, 0x35, 0xe9, 0x38 };
    const int kSpsSize = 18;
    const int kPpsSize = 4;

    std::vector<uint8_t> extradata = buildExtraData(frameData, sizeof(frameData));
    ASSERT_EQ(extradata.size(), 33);
    ASSERT_EQ(extradata[6], 0); // sps size first byte
    ASSERT_EQ(extradata[7], kSpsSize); // sps size second byte
    ASSERT_EQ(memcmp(extradata.data() + 8, frameData + 4, kSpsSize), 0);
    ASSERT_EQ(extradata[8 + kSpsSize], 1); // pps count
    ASSERT_EQ(extradata[8 + kSpsSize + 1], 0); // pps size first byte
    ASSERT_EQ(extradata[8 + kSpsSize + 2], kPpsSize); // pps size second byte
    ASSERT_EQ(memcmp(extradata.data() + 11 + kSpsSize, frameData + 8 + kSpsSize, kPpsSize), 0);
}
