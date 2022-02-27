#include <string>

#include <gtest/gtest.h>

#include <udt/buffer.h>

namespace test {

class Buffer:
    public ::testing::Test
{
protected:
    using BufferType = ::BasicBuffer<char>;

    template <typename Str, typename... Args>
    Str generate(Args&&... args)
    {
        Str one(std::forward<Args>(args)...);
        for (std::size_t i = 0; i < one.size(); ++i)
            one[i] = (char) i;

        return one;
    }

    void assertBufferMemoryIsValid(BufferType& buf)
    {
        memset(buf.data(), 'x', buf.size());
    }

    void assertSameMemoryIsUsedToHoldData(const BufferType& one, const BufferType& two)
    {
        ASSERT_GE(two.data(), one.data());
        ASSERT_LE(two.data() + two.size(), one.data() + one.size());
    }

    void assertDifferentMemoryIsUsedToHoldData(const BufferType& one, const BufferType& two)
    {
        // Memory regions of one and two must not intersect.
        ASSERT_TRUE(
            two.data() >= one.data() + one.size() ||
            two.data() + two.size() <= one.data());
    }

    template<typename Other>
    void assertEqual(const BufferType& buf, const Other& other)
    {
        ASSERT_EQ(buf.size(), other.size());
        ASSERT_EQ(0, memcmp(buf.data(), other.data(), buf.size()));
    }

    void assertSubstrValid(
        const BufferType& buf,
        std::size_t offset, std::size_t count)
    {
        auto two = buf.substr(offset, count);
        assertSameMemoryIsUsedToHoldData(buf, two);
        ASSERT_EQ(0, memcmp(((const BufferType&)two).data(), buf.data() + offset, count));

        // Causing substr relocation.
        two.data();
        ASSERT_EQ(0, memcmp(((const BufferType&)two).data(), buf.data() + offset, count));
    }
};

TEST_F(Buffer, constructing_preallocated_buffer)
{
    BufferType one(1024, 0);

    ASSERT_EQ(1024, one.size());
    assertBufferMemoryIsValid(one);
}

TEST_F(Buffer, data_shared_implicitely)
{
    BufferType one(1024, 0);
    BufferType two = one;

    assertEqual(one, two);
    assertSameMemoryIsUsedToHoldData(one, two);

    // Non-const method invokation.
    two.data();

    assertEqual(one, two);
    assertDifferentMemoryIsUsedToHoldData(one, two);
}

TEST_F(Buffer, data_modification_causes_realocation)
{
    static constexpr std::string_view kData = "Hello, world";

    BufferType one(kData.data(), kData.size());

    BufferType two(one);
    two[7] = 'W';

    assertEqual(one, std::string_view("Hello, world"));
    assertEqual(two, std::string_view("Hello, World"));
}

TEST_F(Buffer, assign)
{
    static constexpr std::string_view kData = "Hello, world";

    BufferType one(1024, 0);
    one.assign(kData.data(), kData.size());

    assertEqual(one, kData);
}

TEST_F(Buffer, substr)
{
    auto one = generate<BufferType>(1024, '\0');

    assertSubstrValid(one, 0, 1024);
    assertSubstrValid(one, 0, 128);
    assertSubstrValid(one, 1000, 24);
    assertSubstrValid(one, 128, 128);
}

TEST_F(Buffer, resize)
{
    auto data = generate<std::string>(1024, 'x');
    BufferType one(data.data(), data.size());

    one.resize(100);
    data.resize(100);
    assertEqual(one, data);

    one.resize(0);
    ASSERT_EQ(0, one.size());
}

TEST_F(Buffer, resize_down_does_not_reallocate)
{
    auto one = generate<BufferType>(1024, '\0');
    auto oneBuf = one.data();

    one.resize(one.size() / 2);

    ASSERT_EQ(oneBuf, one.data());
}

TEST_F(Buffer, substr_bounds_checking)
{
    auto one = generate<BufferType>(1024, '\0');

    {
        const auto substr = one.substr(1000, 100);
        ASSERT_EQ(substr.size(), 24);
        ASSERT_EQ(0, memcmp(substr.data(), one.data() + 1000, substr.size()));
    }

    {
        const auto substr = one.substr(1000, BufferType::npos);
        ASSERT_EQ(substr.size(), 24);
        ASSERT_EQ(0, memcmp(substr.data(), one.data() + 1000, substr.size()));
    }

    {
        const auto substr = one.substr(1200, 100);
        ASSERT_EQ(substr.size(), 0);
    }
}

TEST_F(Buffer, substr_from_substr)
{
    auto one = generate<BufferType>(1024, '\0');

    const auto substr1 = one.substr(100, 200);
    const auto substr2 = substr1.substr(100, 100);
    ASSERT_EQ(substr2.size(), 100);
    ASSERT_EQ(0, memcmp(substr2.data(), one.data() + 200, substr2.size()));
}

TEST_F(Buffer, overwriting_substr)
{
    static constexpr std::string_view kData = "Hello, world";

    auto one = generate<BufferType>(1024, '\0');

    auto substr = one.substr(100, 200);
    substr.assign(kData.data(), kData.size());
    assertEqual(substr, kData);
}

} // namespace test
