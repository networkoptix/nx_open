#include <gtest/gtest.h>

#include <utils/media/media_stream_cache.h>
#include <utils/media/detail/media_stream_cache_detail.h>
#include <nx/streaming/video_data_packet.h>

// TODO: Is it correct namespaces?
namespace nx {
namespace vms {
namespace server {
namespace test {

using namespace std::chrono;

namespace {

QnCompressedVideoDataPtr createVideoPacket(milliseconds timestamp, bool isKeyFrame = false)
{
    auto packet = std::make_shared<QnWritableCompressedVideoData>();
    packet->timestamp = timestamp.count() * 1000;
    if (isKeyFrame)
        packet->flags |= QnAbstractMediaData::MediaFlags_AVKey;
    return packet;
}

void fillCache(
    MediaStreamCache& cache,
    milliseconds timestampStart,
    milliseconds timestampStep,
    int keyFrameStep,
    int dataPacketsNumber,
    std::function<void(MediaStreamCache&)> handler = {})
{
    milliseconds currentTimestamp = timestampStart;
    for (int i = 0; i < dataPacketsNumber; ++i)
    {
        cache.putData(createVideoPacket(currentTimestamp, (i % keyFrameStep == 0)));
        currentTimestamp += timestampStep;
    }
}

void assertSearch(MediaStreamCache& cache,
    milliseconds searchedTimestamp, milliseconds shouldFindTimestamp, int channel = 0)
{
    quint64 foundTimestamp;
    auto dataPacket =
        cache.findByTimestamp(searchedTimestamp.count() * 1000, false, &foundTimestamp, channel);
    ASSERT_NE(dataPacket, nullptr);
    EXPECT_EQ(dataPacket->timestamp, shouldFindTimestamp.count() * 1000);
    EXPECT_EQ(foundTimestamp, shouldFindTimestamp.count() * 1000);

    auto videoPacket = std::dynamic_pointer_cast<QnCompressedVideoData>(dataPacket);
    ASSERT_NE(videoPacket, nullptr);
    EXPECT_EQ(videoPacket->channelNumber, channel);
}

} // namespace

// TODO: test channels and keyFrameOnly
// TODO: add test for two same numbers
// TODO: add check if we are too far from required value
TEST(MediaStreamCache, findByTimestamp)
{
    const std::chrono::milliseconds cacheSize(1000);
    MediaStreamCache cache(cacheSize.count(), cacheSize.count() * 10);

    const milliseconds tsStart(10);
    const milliseconds tsStep(10);
    const int cachePacketsNumber = 8;
    const milliseconds tsLast(tsStart + tsStep * (cachePacketsNumber - 1));

    fillCache(cache, tsStart, tsStep, 3, cachePacketsNumber);

    // The first one.
    assertSearch(cache, tsStart, tsStart, 0);
    // The one inside.
    assertSearch(cache, tsStart + tsStep, tsStart + tsStep, 0);
    // The one inside, no exact match.
    assertSearch(cache, tsStart + milliseconds(1), tsStart, 0);
    // The last one.
    assertSearch(cache, tsLast, tsLast, 0);
    // Before the last one, no exact match.
    assertSearch(cache, tsLast - milliseconds(1), tsLast - tsStep, 0);
    // After the last one, close to the last one.
    assertSearch(cache, tsLast + milliseconds(1), tsLast, 0);

    quint64 foundTimestamp;
    // Before the first one.
    ASSERT_EQ(cache.findByTimestamp((tsStart.count() - 1) * 1000, false, &foundTimestamp, 0), nullptr);
//     After the last one, far from the last.
    auto tsFarFromLast = tsLast + detail::MediaStreamCache::kMaxTimestampDeviation + milliseconds(1);
    ASSERT_EQ(cache.findByTimestamp(tsFarFromLast.count() * 1000, false, &foundTimestamp, 0), nullptr);
}

//TEST(MediaStreamCache, cacheObsolescence)
//{
//    // TODO
//}


} // namespace test
} // namespace server
} // namespace vms
} // namespace nx
