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

struct FrameDescriptor
{
    milliseconds timestamp{0};
    int channelNumber = 0;
    bool isKeyFrame = false;
};

// Sequence: K0 p0 p0 K0 p0 p0 K0 p0.
const std::vector<FrameDescriptor> singleChannelTestData =
{
    {milliseconds(10), 0, true}, {milliseconds(20), 0, false}, {milliseconds(30), 0, false},
    {milliseconds(40), 0, true}, {milliseconds(50), 0, false}, {milliseconds(60), 0, false},
    {milliseconds(70), 0, true}, {milliseconds(80), 0, false}
};

// Sequence: K0 K1 p0 p1 K1 K0 p0 p1 K0 p1 K1.
const std::vector<FrameDescriptor> multiChannelTestData =
{
    {milliseconds(10), 0, true},  // K0
    {milliseconds(10), 1, true},  // K1
    {milliseconds(20), 0, false}, // p0
    {milliseconds(30), 1, false}, // p1

    {milliseconds(30), 1, true},  // K1
    {milliseconds(30), 0, true},  // K0
    {milliseconds(40), 0, false}, // p0
    {milliseconds(40), 1, false}, // p1

    {milliseconds(50), 0, true},  // K0
    {milliseconds(60), 1, false}, // p1
    {milliseconds(70), 1, true}   // K1
};

} // namespace

class MediaStreamCacheTest : public ::testing::Test
{
public:
    const milliseconds cacheSize{1000};

    MediaStreamCache cache;
    milliseconds tsStart;
    milliseconds tsLast;

public:
    MediaStreamCacheTest() : cache(cacheSize.count(), cacheSize.count() * 10) {}

    void fillCache(const std::vector<FrameDescriptor>& cacheData,
        std::function<void(MediaStreamCache&)> handler = {})
    {
        tsStart = cacheData.begin()->timestamp;
        tsLast = (--cacheData.end())->timestamp;

        for (const auto frame: cacheData)
        {
            cache.putData(createVideoPacket(frame.timestamp, frame.isKeyFrame, frame.channelNumber));
            if (handler)
                handler(cache);
        }
    }

    void assertSearch(milliseconds searchedTimestamp, milliseconds shouldFindTimestamp,
        int channel = 0, bool isKeyFrameOnly = false)
    {
        quint64 foundTimestamp;
        auto dataPacket =
            cache.findByTimestamp(searchedTimestamp.count() * 1000, isKeyFrameOnly, &foundTimestamp, channel);
        ASSERT_NE(dataPacket, nullptr);
        EXPECT_EQ(dataPacket->timestamp, shouldFindTimestamp.count() * 1000);
        EXPECT_EQ(foundTimestamp, shouldFindTimestamp.count() * 1000);

        auto videoPacket = std::dynamic_pointer_cast<QnCompressedVideoData>(dataPacket);
        ASSERT_NE(videoPacket, nullptr);
        EXPECT_EQ(videoPacket->channelNumber, channel);
    }

private:
    QnCompressedVideoDataPtr createVideoPacket(
        milliseconds timestamp, bool isKeyFrame = false, int channel = 0)
    {
        auto packet = std::make_shared<QnWritableCompressedVideoData>();
        packet->channelNumber = channel;
        packet->timestamp = timestamp.count() * 1000;
        if (isKeyFrame)
            packet->flags |= QnAbstractMediaData::MediaFlags_AVKey;
        return packet;
    }
};

TEST_F(MediaStreamCacheTest, findByTimestamp)
{
    const milliseconds tsInside = (++singleChannelTestData.begin())->timestamp;
    const milliseconds tsBeforeLast =
        (singleChannelTestData[singleChannelTestData.size()-2]).timestamp;

    fillCache(singleChannelTestData);

    // The first one.
    assertSearch(tsStart, tsStart);
    // The one inside.
    assertSearch(tsInside, tsInside);
    // The one inside, no exact match.
    assertSearch(tsStart + milliseconds(1), tsStart);
    // The last one.
    assertSearch(tsLast, tsLast);
    // Before the last one, no exact match.
    assertSearch(tsLast - milliseconds(1), tsBeforeLast);
    // After the last one, close to the last one.
    assertSearch(tsLast + milliseconds(1), tsLast);

    quint64 foundTimestamp;
    // Before the first one.
    ASSERT_EQ(cache.findByTimestamp((tsStart.count() - 1) * 1000, false, &foundTimestamp, 0), nullptr);
//     After the last one, far from the last.
    auto tsFarFromLast = tsLast + detail::MediaStreamCache::kMaxTimestampDeviation + milliseconds(1);
    ASSERT_EQ(cache.findByTimestamp(tsFarFromLast.count() * 1000, false, &foundTimestamp, 0), nullptr);
}

// TODO: test channels and keyFrameOnly
// TODO: add test for two same numbers
TEST_F(MediaStreamCacheTest, findByTimestamp_multichannel)
{
    fillCache(multiChannelTestData);
}

//TEST(MediaStreamCache, getNextPacket)
//{
//    // TODO
//}


} // namespace test
} // namespace server
} // namespace vms
} // namespace nx
