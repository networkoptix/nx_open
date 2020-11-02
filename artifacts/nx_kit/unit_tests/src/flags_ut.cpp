// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <nx/kit/test.h>
#include <nx/kit/flags.h>

#include <stdint.h>

namespace nx {
namespace kit {
namespace test {

enum class Color: uint8_t
{
    black = 0,

    red = 1 << 0,
    green = 1 << 1,
    blue = 1 << 2,

    yellow = red | green,
    magenta = red | blue,
    cyan = green | blue,

    white = red | green | blue,
};
NX_KIT_ENABLE_FLAGS(Color);

struct Border
{
    enum CrossDirection
    {
        NoDirection = 0,
        InDirection = 1 << 0,
        OutDirection = 1 << 1,
        EitherDirection = InDirection | OutDirection,
    };
};
NX_KIT_ENABLE_FLAGS(Border::CrossDirection);

static_assert(std::is_same<decltype(~Border::NoDirection), Border::CrossDirection>::value,
    "Result of applying bitwise operator to a flags enum must have the same type as the argument");

TEST(flags, intersect)
{
    ASSERT_TRUE((Color::yellow & Color::cyan) == Color::green);
    {
        Color c = Color::yellow;
        c &= Color::blue;
        ASSERT_TRUE(c == Color::black);
    }

    ASSERT_TRUE(Border::InDirection & Border::EitherDirection);
    {
        Border::CrossDirection d = Border::EitherDirection;
        d &= Border::InDirection;
        ASSERT_TRUE(d == Border::InDirection);
    }
}

TEST(flags, subtract)
{
    ASSERT_TRUE((Color::yellow & ~Color::green) == Color::red);
    {
        Color c = Color::cyan;
        c &= ~Color::blue;
        ASSERT_TRUE(c == Color::green);
    }

    ASSERT_TRUE((Border::EitherDirection & ~Border::OutDirection) == Border::InDirection);
    {
        Border::CrossDirection d = Border::EitherDirection;
        d &= ~Border::InDirection;
        ASSERT_TRUE(d == Border::OutDirection);
    }
}

TEST(flags, unite)
{
    ASSERT_TRUE((Color::red | Color::blue) == Color::magenta);
    {
        Color c = Color::red;
        c |= Color::cyan;
        ASSERT_TRUE(c == Color::white);
    }

    ASSERT_TRUE((Border::InDirection | Border::OutDirection) == Border::EitherDirection);
    {
        Border::CrossDirection d = Border::NoDirection;
        d |= Border::InDirection;
        ASSERT_TRUE(d == Border::InDirection);
    }
}

TEST(flags, none)
{
    ASSERT_TRUE(!Color::black);
    ASSERT_TRUE(!Border::NoDirection);
}

TEST(flags, any)
{
    ASSERT_TRUE(!!Color::blue);
    ASSERT_TRUE((bool) Color::blue);

    ASSERT_TRUE(!!Border::InDirection);
    ASSERT_TRUE((bool) Border::InDirection);
    ASSERT_TRUE(Border::InDirection);
}

} // namespace test
} // namespace kit
} // namespace nx
