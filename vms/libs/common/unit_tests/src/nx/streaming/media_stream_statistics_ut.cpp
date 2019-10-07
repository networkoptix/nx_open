#include <gtest/gtest.h>

#include <nx/streaming/video_data_packet.h>
#include <nx/streaming/media_stream_statistics.h>

namespace nx::streaming::test {

TEST(MediaStreamStatistics, main)
{
    QnMediaStreamStatistics statistics;

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
    ASSERT_EQ(10000000, statistics.bitrateBitsPerSecond());
    ASSERT_FLOAT_EQ(25, statistics.getFrameRate());
}

} // namespace nx::streaming::test
