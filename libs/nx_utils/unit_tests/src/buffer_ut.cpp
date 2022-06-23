// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/buffer.h>
#include <nx/utils/type_utils.h>

namespace nx {

void PrintTo(const Buffer& val, ::std::ostream* os)
{
    *os << std::string(val.data(), val.size());
}

} // namespace nx

namespace nx::test {

TEST(Buffer, instantiation)
{
    ASSERT_EQ("Hello, world", (std::string_view) Buffer("Hello, world"));
    ASSERT_EQ("Hello", (std::string_view) Buffer("Hello, world", 5));
    ASSERT_EQ(
        "More than 40 bytes. xxxxxxxxxxxxxxxxxxxxxxxx",
        (std::string_view) Buffer("More than 40 bytes. xxxxxxxxxxxxxxxxxxxxxxxx"));
    ASSERT_EQ("Hello, world", (std::string_view) Buffer(QByteArray("Hello, world")));
    ASSERT_EQ("Hello, world", (std::string_view) Buffer(std::string("Hello, world")));

    ASSERT_EQ(std::string(10, 'x'), Buffer(10, 'x'));
    ASSERT_EQ(std::string(99, 'x'), Buffer(99, 'x'));
}

TEST(Buffer, copying)
{
    Buffer buf("Hello, world");
    Buffer buf2(buf);
    buf = "Bye, cruel world";

    ASSERT_EQ("Hello, world", buf2);
    ASSERT_EQ("Bye, cruel world", buf);
}

TEST(Buffer, moving)
{
    Buffer buf("Hello, world");
    Buffer buf2(std::move(buf));
    buf = Buffer("Bye, cruel world");

    ASSERT_EQ("Hello, world", buf2);
    ASSERT_EQ("Bye, cruel world", buf);
}

TEST(Buffer, reserve)
{
    Buffer buf("Hello, world");

    buf.reserve(buf.capacity() * 2);
    ASSERT_EQ("Hello, world", buf);
}

TEST(Buffer, resize)
{
    for (std::size_t x: {10, 40, 100})
    {
        Buffer buf;
        buf.resize(x, 'x');
        ASSERT_EQ(std::string(x, 'x'), buf);
    }
}

TEST(Buffer, swap)
{
    auto testData = std::make_tuple(QByteArray(100, 'x'), std::string(100, 'x'), "Hello");
    nx::utils::tuple_for_each(
        testData,
        [](const auto& data)
        {
            nx::Buffer one(data);
            nx::Buffer two("raz, raz");

            one.swap(two);

            ASSERT_EQ(data, two);
            ASSERT_EQ("raz, raz", one);
        });
}

TEST(Buffer, append)
{
    auto testData = std::make_tuple(QByteArray(100, 'x'), std::string(100, 'x'), "Hello");
    nx::utils::tuple_for_each(
        testData,
        [](const auto& data)
        {
            nx::Buffer buf(data);
            buf.append(", world");

            if constexpr (std::is_same_v<std::remove_cv_t<decltype(data)>, QByteArray>)
                ASSERT_EQ(data + ", world", buf);
            else
                ASSERT_EQ(std::string(data) + ", world", buf);
        });
}

TEST(Buffer, find)
{
    Buffer buf("Hello, world, world");

    ASSERT_EQ(7, buf.find("world"));
    ASSERT_EQ(Buffer::npos, buf.find("cruel"));

    ASSERT_EQ(1, buf.find_first_of("el"));
    ASSERT_EQ(Buffer::npos, buf.find_first_of("xyz"));

    ASSERT_EQ(2, buf.find_first_not_of("He"));
    ASSERT_EQ(4, buf.find_first_not_of("Hel"));
    ASSERT_EQ(Buffer::npos, buf.find_first_not_of("Hello, world"));

    ASSERT_EQ(14, buf.rfind("world"));
    ASSERT_EQ(Buffer::npos, buf.rfind("cruel"));

    ASSERT_EQ(17, buf.find_last_of("worl"));
    ASSERT_EQ(Buffer::npos, buf.find_last_of("xyz"));
}

TEST(Buffer, to_string_view)
{
    Buffer buf("Hello, world");

    ASSERT_EQ("Hello, world", (std::string_view) buf);
}

TEST(Buffer, substr)
{
    Buffer buf("Hello, world");

    ASSERT_EQ("Hello, world", buf.substr());
    ASSERT_EQ("Hello", buf.substr(0, 5));
    ASSERT_EQ("world", buf.substr(7));
    ASSERT_EQ("world", buf.substr(7, 100));
}

TEST(Buffer, replace)
{
    Buffer buf("Hello, world");

    ASSERT_EQ("Hello, guy", Buffer("Hello, world").replace(7, 5, "guy"));
    ASSERT_EQ("Hello, guy", Buffer("Hello, world").replace(7, Buffer::npos, "guy"));
    ASSERT_EQ("Big Hello, world", Buffer("Hello, world").replace(0, 0, "Big "));
    ASSERT_EQ("Hello, world!", Buffer("Hello, world").replace(buf.size(), 0, "!"));
    ASSERT_EQ("Good morning, world", Buffer("Hello, world").replace(0, 5, "Good morning"));
    ASSERT_EQ("Hi, world", Buffer("Hello, world").replace(0, 5, "Hi"));
    ASSERT_EQ("Hello, beautiful world", Buffer("Hello, world").replace(7, 0, "beautiful "));
    ASSERT_EQ("Chiao, mondo", Buffer("Hello, world").replace(0, buf.size(), "Chiao, mondo"));
}

TEST(Buffer, iterating)
{
    Buffer buf("Hello, world");
    std::string str;
    std::copy(buf.begin(), buf.end(), std::back_inserter(str));
    ASSERT_EQ(buf, str);
}

TEST(Buffer, concatenation)
{
    ASSERT_EQ("Hello, world", nx::Buffer("Hello, ") + "world");
    ASSERT_EQ("Hello, world", "Hello, " + nx::Buffer("world"));

    ASSERT_EQ("Hello, world", nx::Buffer("Hello, ") + std::string("world"));
    ASSERT_EQ("Hello, world", std::string("Hello, ") + nx::Buffer("world"));
}

TEST(Buffer, contains)
{
    ASSERT_TRUE(nx::Buffer("Hello, world").contains("Hello"));
    ASSERT_TRUE(nx::Buffer("Hello, world").contains("world"));
    ASSERT_TRUE(nx::Buffer("Hello, world").contains(std::string("Hello")));
    ASSERT_TRUE(nx::Buffer("Hello, world").contains(std::string_view("world")));

    ASSERT_FALSE(nx::Buffer("Hello, world").contains("Bye"));
    ASSERT_FALSE(nx::Buffer("Hello, world").contains("Hlw"));
}

TEST(Buffer, erase)
{
    ASSERT_EQ("world", nx::Buffer("Hello, world").erase(0, 7));
    ASSERT_EQ("Hello", nx::Buffer("Hello, world").erase(5));

    ASSERT_EQ("world", nx::Buffer(QByteArray("Hello, world")).erase(0, 7));
    ASSERT_EQ("Hello", nx::Buffer(QByteArray("Hello, world")).erase(5));

    ASSERT_EQ("world", nx::Buffer(std::string("Hello, world")).erase(0, 7));
    ASSERT_EQ("Hello", nx::Buffer(std::string("Hello, world")).erase(5));
}

TEST(Buffer, assign)
{
    nx::Buffer buf("Hello, world");
    buf = (const char*) nullptr;
    buf = {};
    buf = nx::Buffer("Hello, world");
}

TEST(Buffer, base64_encoding_is_invertible)
{
    nx::Buffer buf("Hello, world");
    ASSERT_EQ(buf, nx::Buffer::fromBase64(buf.toBase64()));
}

} // namespace nx::test
