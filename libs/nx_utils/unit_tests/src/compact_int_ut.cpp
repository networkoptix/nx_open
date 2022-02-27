// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <algorithm>
#include <chrono>
#include <thread>
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

//-------------------------------------------------------------------------------------------------

struct Compacted
{
    long long f1 = 0;
    long long f2 = 0;
    long long f3 = 0;

    bool deserialize(const char** data, const char* dataEnd)
    {
        return compact_int::deserialize(data, dataEnd, &f1)
            && compact_int::deserialize(data, dataEnd, &f2)
            && compact_int::deserialize(data, dataEnd, &f3);
    }

    void serialize(QByteArray* buf) const
    {
        compact_int::serialize(f1, buf);
        compact_int::serialize(f2, buf);
        compact_int::serialize(f3, buf);
    }
};

struct Regular
{
    long long f1 = 0;
    long long f2 = 0;
    long long f3 = 0;

    bool deserialize(const char** data, const char* dataEnd)
    {
        if (std::size_t(dataEnd - *data) < sizeof(f1) * 3)
            return false;

        memcpy(&f1, *data, sizeof(f1));
        *data += sizeof(f1);
        memcpy(&f2, *data, sizeof(f2));
        *data += sizeof(f2);
        memcpy(&f3, *data, sizeof(f3));
        *data += sizeof(f3);

        return true;
    }

    void serialize(QByteArray* buf) const
    {
        buf->append((const char*) &f1, sizeof(f1));
        buf->append((const char*) &f2, sizeof(f2));
        buf->append((const char*) &f3, sizeof(f3));
    }
};

TEST_F(CompactInt, DISABLED_performance)
{
    using Data = Compacted;
    using namespace std::chrono;

    static constexpr int count = 1000000;

    std::vector<Data> items;
    items.resize(count);
    std::for_each(
        items.begin(), items.end(),
        [](auto& item)
        {
            item.f1 = rand();
            item.f2 = rand();
            item.f3 = rand();
        });

    QByteArray data;
    data.reserve(items.size() * sizeof(Data));

    const auto t0 = std::chrono::steady_clock::now();

    std::for_each(
        items.begin(), items.end(),
        [&data](const auto& item)
        {
            item.serialize(&data);
        });

    const auto t1 = std::chrono::steady_clock::now();

    std::cout << "Serializing " << items.size() << " items took " <<
        duration_cast<milliseconds>(t1 - t0).count() << "ms, " <<
        data.size() << " bytes written" << std::endl;

    const auto t2 = std::chrono::steady_clock::now();

    const char* begin = data.data();
    const char* end = data.data() + data.size();
    bool result = true;

    std::for_each(
        items.begin(), items.end(),
        [&begin, end, &result](auto& item)
        {
            result &= item.deserialize(&begin, end);
        });

    ASSERT_TRUE(result);

    const auto t3 = std::chrono::steady_clock::now();

    std::cout << "Deserializing " << items.size() << " items took " <<
        duration_cast<milliseconds>(t3 - t2).count() << "ms" << std::endl;

    std::this_thread::sleep_for(std::chrono::minutes(1));
}

} // namespace nx::utils::compact_int::test
