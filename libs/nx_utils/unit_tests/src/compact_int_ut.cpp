#include <algorithm>
#include <type_traits>

#include <gtest/gtest.h>

#include <nx/utils/compact_int.h>
#include <nx/utils/random.h>

namespace nx::utils::compact_int::test {

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
            deserialized<typename std::remove_const<decltype(value)>::type>(serialized(value));

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

} // namespace nx::utils::compact_int::test
