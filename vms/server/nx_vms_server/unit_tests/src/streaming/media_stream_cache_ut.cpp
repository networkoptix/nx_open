#include <gtest/gtest.h>

#include <utils/media/media_stream_cache.h>
#include <utils/media/detail/media_stream_cache_detail.h>
#include <nx/streaming/video_data_packet.h>

namespace nx::vms::server::test {

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

    {milliseconds(40), 1, true},  // K1
    {milliseconds(40), 0, true},  // K0
    {milliseconds(50), 0, false}, // p0
    {milliseconds(50), 1, false}, // p1

    {milliseconds(60), 0, true},  // K0
    {milliseconds(70), 1, false}, // p1
    {milliseconds(80), 1, true}   // K1
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

    void assertSearch(milliseconds searchedTimestamp, std::optional<milliseconds> shouldFindTimestamp,
        int channel = 0, bool isKeyFrameOnly = false)
    {
        quint64 foundTimestamp;
        auto dataPacket = cache.findByTimestamp(
            searchedTimestamp.count() * 1000, isKeyFrameOnly, &foundTimestamp, channel);

        if (!shouldFindTimestamp.has_value())
        {
            ASSERT_EQ(dataPacket, nullptr);
            return;
        }
        ASSERT_NE(dataPacket, nullptr);
        EXPECT_EQ(dataPacket->timestamp, shouldFindTimestamp->count() * 1000);
        EXPECT_EQ(foundTimestamp, shouldFindTimestamp->count() * 1000);

        auto videoPacket = std::dynamic_pointer_cast<QnCompressedVideoData>(dataPacket);
        ASSERT_NE(videoPacket, nullptr);
        if (isKeyFrameOnly)
        {
            EXPECT_TRUE(videoPacket->flags.testFlag(QnAbstractMediaData::MediaFlags_AVKey));
        }
        EXPECT_EQ(videoPacket->channelNumber, channel);
    }

    void assertNextPacket(milliseconds startingTimestamp,
        std::optional<milliseconds> shouldFindTimestamp, int channel)
    {
        quint64 foundTimestamp;
        auto dataPacket = cache.getNextPacket(
            startingTimestamp.count() * 1000, &foundTimestamp, channel);

        if (!shouldFindTimestamp.has_value())
        {
            ASSERT_EQ(dataPacket, nullptr);
            return;
        }
        ASSERT_NE(dataPacket, nullptr);
        EXPECT_EQ(dataPacket->timestamp, shouldFindTimestamp->count() * 1000);
        EXPECT_EQ(foundTimestamp, shouldFindTimestamp->count() * 1000);

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

    // Search in empty cache.
    assertSearch(tsStart, {});

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

    // Before the first one.
    assertSearch(tsStart - milliseconds(1), {});
    // After the last one, far from the last.
    auto tsFarFromLast = tsLast + detail::MediaStreamCache::kMaxTimestampDeviation + milliseconds(1);
    assertSearch(tsFarFromLast, {});
}

TEST_F(MediaStreamCacheTest, findByTimestamp_multichannel)
{
    const auto &testData = multiChannelTestData;
    fillCache(testData);

    // Exactly key-frame.
    assertSearch(testData[0].timestamp, testData[0].timestamp, 0, /*isKeyFrameOnly*/ true);
    assertSearch(testData[1].timestamp, testData[1].timestamp, 1, /*isKeyFrameOnly*/ true);

    // Exactly p-frame.
    assertSearch(testData[2].timestamp, testData[0].timestamp, 0, /*isKeyFrameOnly*/ true);
    assertSearch(testData[3].timestamp, testData[1].timestamp, 1, /*isKeyFrameOnly*/ true);

    // Another channel key-frame.
    assertSearch(testData[8].timestamp, testData[4].timestamp, 1, /*isKeyFrameOnly*/ true);
    assertSearch(testData[10].timestamp, testData[8].timestamp, 0, /*isKeyFrameOnly*/ true);

    // Another channel p-frame (isKeyFrameOnly = false).
    assertSearch(testData[7].timestamp, testData[6].timestamp, 0, /*isKeyFrameOnly*/ false);
    assertSearch(testData[8].timestamp, testData[7].timestamp, 1, /*isKeyFrameOnly*/ false);

    // After the last one.
    assertSearch(tsLast + milliseconds(1), testData[8].timestamp, 0, /*isKeyFrameOnly*/ true);
    assertSearch(tsLast + milliseconds(1), testData[10].timestamp, 1, /*isKeyFrameOnly*/ true);

    // No exact match.
    assertSearch(tsLast - milliseconds(1), testData[8].timestamp, 0, /*isKeyFrameOnly*/ true);
    assertSearch(tsLast - milliseconds(1), testData[4].timestamp, 1, /*isKeyFrameOnly*/ true);

    // Not found channel.
    assertSearch(tsLast, {}, 2, /*isKeyFrameOnly*/ true);
}

TEST_F(MediaStreamCacheTest, getNextPacket)
{
    const auto &testData = multiChannelTestData;
    fillCache(testData);

    const auto assertChannel =
        [this, testData](const std::vector<int>& channelIndexes, int channel)
        {
            for (size_t i = 1; i < channelIndexes.size(); ++i)
            {
                const auto tsStart = testData[channelIndexes[i-1]].timestamp;
                const auto tsExpectedNext = testData[channelIndexes[i]].timestamp;
                assertNextPacket(tsStart, tsExpectedNext, channel);
            }
            assertNextPacket(testData[channelIndexes.back()].timestamp, {}, channel);
        };

    assertChannel({0, 2, 5, 6, 8}, 0);
    assertChannel({1, 3, 4, 7, 9, 10}, 1);
}

} // namespace nx::vms::server::test
