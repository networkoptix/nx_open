#include <gtest/gtest.h>

#include <nx/streaming/video_data_packet.h>
#include <nx/streaming/media_stream_statistics.h>

namespace nx::streaming::test {

void check(int maxFrames, int overlappedBitrate, int overlappedFps)
{
    QnMediaStreamStatistics statistics;
    statistics.setWindowSize(std::chrono::seconds(2));
    statistics.setMaxDurationInFrames(maxFrames);

    QnWritableCompressedVideoDataPtr video(new QnWritableCompressedVideoData());
    const auto currentTimeMs = 0;
    video->timestamp = currentTimeMs;
    video->m_data.resize(1000 * 50);

    statistics.onData(video);

    ASSERT_EQ(0, statistics.bitrateBitsPerSecond());
    ASSERT_EQ(0, statistics.getFrameRate());

    for (int i = 0; i < 100; ++i)
    {
        video->timestamp += 50000; //< 20 fps, 1Mb/sec
        statistics.onData(video);

        ASSERT_EQ(8000000, statistics.bitrateBitsPerSecond());
        ASSERT_FLOAT_EQ(20, statistics.getFrameRate());
    }

    // Generate hole
    video->timestamp += 1000000 * 10;
    statistics.onData(video);
    ASSERT_EQ(0, statistics.bitrateBitsPerSecond());
    ASSERT_EQ(0, statistics.getFrameRate());

    for (int i = 0; i < 100; ++i)
    {
        video->timestamp += 2 * 50000; //< 10 fps, 1Mb/sec
        statistics.onData(video);

        ASSERT_EQ(4000000, statistics.bitrateBitsPerSecond());
        ASSERT_FLOAT_EQ(10, statistics.getFrameRate());
    }

    video->timestamp = 0;
    statistics.onData(video);
    ASSERT_EQ(0, statistics.bitrateBitsPerSecond());
    ASSERT_EQ(0, statistics.getFrameRate());

    for (int i = 0; i < 100; ++i)
    {
        video->timestamp += 50000; //< 20 fps, 1Mb/sec
        statistics.onData(video);
        ASSERT_EQ(8000000, statistics.bitrateBitsPerSecond());
        ASSERT_FLOAT_EQ(20, statistics.getFrameRate());
    }

    video->timestamp -= 1000000;
    for (int i = 0; i < 10; ++i)
    {
        statistics.onData(video);
        video->timestamp += 50000; //< 20 fps, 1Mb/sec
    }
    ASSERT_EQ(overlappedBitrate, statistics.bitrateBitsPerSecond());
    ASSERT_FLOAT_EQ(overlappedFps, statistics.getFrameRate());
}

TEST(MediaStreamStatistics, main)
{
    check(/*maxFrames */ 1000, /*overlappedBitrate*/ 10000000, /*overlappedFps*/ 25);
    check(/*maxFrames */ 10, /*overlappedBitrate*/ 8000000, /*overlappedFps*/ 20);
}

} // namespace nx::streaming::test
