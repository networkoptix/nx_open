#include <memory>
#include <sstream>

#include <nx/kit/test.h>
#include <nx/kit/debug.h>

#include <rects_serialization.h>
#include <cstring>
#include <fstream>
#include <algorithm>
#include <ctime>

using Rect = TegraVideo::Rect;

static void checkRectsEq(const Rect& expected, const Rect& actual)
{
    ASSERT_EQ(expected.x, actual.x);
    ASSERT_EQ(expected.y, actual.y);
    ASSERT_EQ(expected.w, actual.w);
    ASSERT_EQ(expected.h, actual.h);
}

static void testRects(const Rect rects[], int rectCount)
{
    const std::string filename = std::string(nx::kit::test::tempDir()) + "rects.txt";

    ASSERT_TRUE(writeRectsToFile(filename, rects, rectCount));

    const int maxRectCount = rectCount + 100;
    std::vector<Rect> newRects(maxRectCount);

    int newRectCount = -1;
    ASSERT_TRUE(readRectsFromFile(filename, &newRects.front(), maxRectCount, &newRectCount));

    ASSERT_EQ(rectCount, newRectCount);
    for (int i = 0; i < rectCount; ++i)
        checkRectsEq(rects[i], newRects[i]);
}

TEST(rects_serialization, twoRects)
{
    Rect rects[2];

    rects[0].x = 0.23F;
    rects[0].y = 0.373737F;
    rects[0].w = 0.19F;
    rects[0].h = 0.11F;
    rects[1].x = 0.0F;
    rects[1].y = 0.0F;
    rects[1].w = 1.0F;
    rects[1].h = 1.0F;

    testRects(rects, sizeof(rects) / sizeof(rects[0]));
}

static void testModulus(int64_t modulus)
{
    const std::string filename = std::string(nx::kit::test::tempDir()) + "modulus.txt";

    ASSERT_TRUE(writeModulusToFile(filename, modulus));

    int64_t modulusFromFile = -1;
    ASSERT_TRUE(readModulusFromFile(filename, &modulusFromFile));
    ASSERT_EQ(modulus, modulusFromFile);
}

TEST(rects_serialization, modulus)
{
    testModulus(INT64_MIN);
    testModulus(0);
    testModulus(INT64_MAX);
}
