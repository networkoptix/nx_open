#include <algorithm>
#include <type_traits>

#include <gtest/gtest.h>

#include <nx/utils/random.h>

#include <analytics/detected_objects_storage/serializers.h>
#include <test_support/analytics/storage/analytics_storage_types.h>

namespace nx::analytics::storage::test {

class CompactInt:
    public ::testing::Test
{
protected:
    long long generateRandomInteger()
    {
        return nx::utils::random::number<long long>(
            std::numeric_limits<long long>::min(),
            std::numeric_limits<long long>::max());
    }

    std::vector<long long> generateRandomIntegerSequence()
    {
        std::vector<long long> sequence(101, 0);
        std::generate(sequence.begin(), sequence.end(),
            [this]() { return generateRandomInteger(); });

        if (nx::utils::random::number<int>(0, 1))
            sequence.insert(sequence.begin(), 0);

        return sequence;
    }

    template<typename T>
    void assertSerializeAndDeserializeAreSymmetric(T value)
    {
        const auto result =
            compact_int::deserialized<typename std::remove_const<decltype(value)>::type>
                (compact_int::serialized(value));

        ASSERT_EQ(value, result);
    }
};

TEST_F(CompactInt, integer_serialize_and_deserialize_are_symmetric)
{
    assertSerializeAndDeserializeAreSymmetric(0LL);
    assertSerializeAndDeserializeAreSymmetric(127LL);
    assertSerializeAndDeserializeAreSymmetric(1024LL);
    assertSerializeAndDeserializeAreSymmetric(generateRandomInteger());
}

TEST_F(CompactInt, sequence_serialize_and_deserialize_are_symmetric)
{
    assertSerializeAndDeserializeAreSymmetric(generateRandomIntegerSequence());
}

//-------------------------------------------------------------------------------------------------

class AnalyticsDbTrackSerializer:
    public ::testing::Test
{
protected:
    std::vector<ObjectPosition> generateRandomTrack()
    {
        std::vector<ObjectPosition> track;
        track.resize(nx::utils::random::number<int>(1, 17));

        for (auto& position: track)
        {
            position.boundingBox = generateRandomRectf();
            position.timestampUsec = nx::utils::random::number<long long>();
            position.durationUsec = nx::utils::random::number<long long>();
            // TODO: attributes
        }

        return track;
    }

    void assertSerializeAndDeserializeAreSymmetric(
        const std::vector<ObjectPosition>& value)
    {
        const auto result = storage::TrackSerializer::deserialized(
            storage::TrackSerializer::serialized(value));

        ASSERT_EQ(value, result);
    }
};

TEST_F(AnalyticsDbTrackSerializer, serialize_and_deserialize_are_symmetric)
{
    const auto track = generateRandomTrack();
    assertSerializeAndDeserializeAreSymmetric(track);
}

TEST_F(AnalyticsDbTrackSerializer, appended_serialization_results_can_be_deserialized)
{
    std::vector<ObjectPosition> track;
    QByteArray serialized;
    for (int i = 0; i < 3; ++i)
    {
        auto trackPart = generateRandomTrack();
        serialized += storage::TrackSerializer::serialized(trackPart);
        std::move(trackPart.begin(), trackPart.end(), std::back_inserter(track));
    }

    const auto deserialized = storage::TrackSerializer::deserialized(serialized);

    ASSERT_EQ(track, deserialized);
}

} // namespace nx::analytics::storage::test
