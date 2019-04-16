#include <algorithm>
#include <type_traits>

#include <gtest/gtest.h>

#include <nx/utils/random.h>

#include <analytics/detected_objects_storage/serializers.h>

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
    assertSerializeAndDeserializeAreSymmetric(127LL);
    assertSerializeAndDeserializeAreSymmetric(1024LL);
    assertSerializeAndDeserializeAreSymmetric(generateRandomInteger());
}

TEST_F(CompactInt, sequence_serialize_and_deserialize_are_symmetric)
{
    assertSerializeAndDeserializeAreSymmetric(generateRandomIntegerSequence());
}

} // namespace nx::analytics::storage::test
