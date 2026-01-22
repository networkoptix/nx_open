// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <vector>

#include <nx/media/gop_reverser.h>
#include <nx/media/video_frame.h>
#include <nx/media/frame_metadata.h>

namespace nx::media::test {

namespace {

const int kFramesCount = 10;
const int kFrameSize = 4'000'000;

} // namespace

class GopReverserTest: public ::testing::Test
{
public:

    void init(int64_t bufferSize, int frameSize)
    {
        reverser = std::make_unique<GopReverser>(bufferSize, frameSize);

        for (int i = 0; i <= kFramesCount; ++i)
        {
            auto frame = std::make_shared<VideoFrame>(QImage({ 1000,1000 }, QImage::Format_RGBA8888));
            frame->setStartTime(i);
            FrameMetadata metadata;
            metadata.dataType = QnAbstractMediaData::DataType::VIDEO;
            metadata.flags.setFlag(QnAbstractMediaData::MediaFlags_ReverseBlockStart, i == 0 || i == kFramesCount);
            metadata.serialize(frame);
            reverser->push(frame);
            if (i < kFramesCount)
                ASSERT_FALSE((bool)reverser->pop());
        }
    }

    void expectFrames(std::vector<int64_t> values)
    {
        for (const auto& value: values)
        {
            auto frame = reverser->pop();
            ASSERT_TRUE((bool) frame);
            ASSERT_EQ(value, frame->startTime());
        }
    }

    std::unique_ptr<GopReverser> reverser;
};

TEST_F(GopReverserTest, enoughBuffer)
{
    init(kFrameSize * kFramesCount, kFramesCount);
    expectFrames({ 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 });
}

TEST_F(GopReverserTest, notEnoughBuffer1)
{
    init(kFrameSize * (kFramesCount - 1), kFramesCount);
    expectFrames({ 9, 8, 7, 6, /*5,*/ 4, 3, 2, 1, 0 });
}

TEST_F(GopReverserTest, notEnoughBufferFrames1)
{
    init(kFrameSize * kFramesCount, kFramesCount - 1);
    expectFrames({ 9, 8, 7, 6, /*5,*/ 4, 3, 2, 1, 0 });
}

TEST_F(GopReverserTest, notEnoughBuffer2)
{
    init(kFrameSize * (kFramesCount - 2), kFramesCount);
    expectFrames({ 9, 8, /*7,*/ 6, 5, /*4,*/ 3, 2, 1, 0 });
}

TEST_F(GopReverserTest, notEnoughBuffer5)
{
    init(kFrameSize * (kFramesCount - 5), kFramesCount);
    expectFrames({ 9, /*8,*/ 7, /*6, 5,*/ 4, /*3,*/ 2, /*1,*/ 0 });
}

TEST_F(GopReverserTest, notEnoughBuffer6)
{
    init(kFrameSize * (kFramesCount - 6), kFramesCount);
    expectFrames({ 9, 7, 3, 0 });
}

TEST_F(GopReverserTest, notEnoughBuffer9)
{
    init(kFrameSize * (kFramesCount - 9), kFramesCount);
    expectFrames({ 0 });
}

TEST_F(GopReverserTest, notEnoughBuffer10)
{
    init(kFrameSize * (kFramesCount - 10), kFramesCount);
    expectFrames({});
}

} // namespace nx::media::test
