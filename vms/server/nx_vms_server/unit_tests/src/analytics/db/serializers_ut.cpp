#include <algorithm>
#include <type_traits>

#include <gtest/gtest.h>

#include <nx/utils/random.h>

#include <nx/vms/server/analytics/db/serializers.h>

#include "attribute_dictionary.h"
#include "utils.h"

namespace nx::analytics::db::test {

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
            position.timestampUs = nx::utils::random::number<long long>();
            position.durationUs = nx::utils::random::number<long long>();
            // TODO: attributes
        }

        return track;
    }

    void assertSerializeAndDeserializeAreSymmetric(
        const std::vector<ObjectPosition>& value)
    {
        const auto result = db::TrackSerializer::deserialized<std::vector<ObjectPosition>>(
            db::TrackSerializer::serialized(value));

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
        serialized += db::TrackSerializer::serialized(trackPart);
        std::move(trackPart.begin(), trackPart.end(), std::back_inserter(track));
    }

    const auto deserialized =
        db::TrackSerializer::deserialized<decltype(track)>(serialized);

    ASSERT_EQ(track, deserialized);
}

} // namespace nx::analytics::db::test
