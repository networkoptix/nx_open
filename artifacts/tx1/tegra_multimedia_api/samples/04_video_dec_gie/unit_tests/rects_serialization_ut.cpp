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

/**
 * Generate a unique file name to avoid collisions in this test.
 */
class UniqueRectsFile
{
public:
    static std::string tempDir()
    {
        static std::string tempDir;
        if (tempDir.empty())
            tempDir = getTempDir();
        return tempDir;
    }

    UniqueRectsFile(const std::string& prefix)
    {
        static bool randomized = false;
        if (!randomized)
        {
            srand((unsigned int) time(nullptr));
            randomized = true;
        }

        m_filename = tempDir() + prefix + nx::kit::debug::format("%d.txt", rand());
    }

    ~UniqueRectsFile() { std::remove(m_filename.c_str()); }

    std::string filename() const { return m_filename; }

private:
    static std::string getTempDir()
    {
        char tempDir[L_tmpnam] = "";
        ASSERT_TRUE(std::tmpnam(tempDir) != nullptr);

        // Extract dir name from the file path - truncate after the last path separator.
        for (int i = (int) strlen(tempDir) - 1; i >= 0; --i)
        {
            if (tempDir[i] == '/' || tempDir[i] == '\\')
            {
                tempDir[i + 1] = '\0';
                break;
            }
        }

        NX_PRINT << "Using temp path: " << tempDir;
        return tempDir;
    }

private:
    std::string m_filename;
};

static void testRects(const Rect rects[], int rectCount)
{
    const UniqueRectsFile file("rects_");

    ASSERT_TRUE(writeRectsToFile(file.filename(), rects, rectCount));

    const int maxRectCount = rectCount + 100;
    std::vector<Rect> newRects(maxRectCount);

    int newRectCount = -1;
    ASSERT_TRUE(readRectsFromFile(
        file.filename(), &newRects.front(), maxRectCount, &newRectCount));

    ASSERT_EQ(rectCount, newRectCount);
    for (int i = 0; i < rectCount; ++i)
        checkRectsEq(rects[i], newRects[i]);
}

TEST(rects_serialization, twoRects)
{
    Rect rects[2];

    rects[0].x = 0.23;
    rects[0].y = 0.373737;
    rects[0].w = 0.19;
    rects[0].h = 0.11;
    rects[1].x = 0;
    rects[1].y = 0;
    rects[1].w = 1;
    rects[1].h = 1;

    testRects(rects, sizeof(rects) / sizeof(rects[0]));
}
