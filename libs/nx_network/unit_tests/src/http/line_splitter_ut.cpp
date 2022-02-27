// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <vector>

#include <gtest/gtest.h>

#include <nx/network/http/line_splitter.h>

namespace nx::network::http::test {

// TODO: #akolesnikov These tests should be re-written.

class LineSplitter:
    public ::testing::Test
{
protected:
    void splitToLines(
        nx::network::http::LineSplitter* lineSplitter,
        const nx::Buffer& sourceText,
        std::vector<nx::Buffer>* lines)
    {
        for (std::size_t pos = 0; pos < sourceText.size(); )
        {
            nx::ConstBufferRefType readLine;
            size_t bytesRead = 0;
            if (lineSplitter->parseByLines(
                    nx::ConstBufferRefType(sourceText.data() + pos, sourceText.size() - pos),
                    &readLine,
                    &bytesRead))
            {
                lines->push_back(nx::Buffer(readLine));
            }
            pos += bytesRead;
        }
    }
};

#if 0
//TODO #akolesnikov uncomment and fix
TEST(LineSplitter, GeneralTest)
{
    static const nx::Buffer testData =
        "line1\r"
        "line2\n"
        "line3\r\n"
        "\r\n"
        "\n"
        "\r"
        "line4\n"
        ;
    std::vector<nx::Buffer> lines;
    splitToLines(testData, &lines);
    ASSERT_EQ(7, lines.size());
    ASSERT_EQ("line1", lines[0]);
    ASSERT_EQ("line2", lines[1]);
    ASSERT_EQ("line3", lines[2]);
    ASSERT_TRUE(lines[3].isEmpty());
    ASSERT_TRUE(lines[4].isEmpty());
    ASSERT_TRUE(lines[5].isEmpty());
    ASSERT_EQ("line4", lines[6]);
}
#endif

TEST_F(LineSplitter, TrailingLFTest)
{
    static const nx::Buffer testData =
        "line1\r\n"
        "line2\r\n"
        "\r\n"
        "xxxxxxxx"
        "xxxxxxxx"
        "xxxxxxxx"
        ;

    nx::network::http::LineSplitter lineSplitter;
    std::vector<nx::Buffer> lines;
    enum State
    {
        readingLines,
        readingTrailingLineEnding,
        done
    }
    state = readingLines;
    std::size_t pos = 0;
    for (; (pos < testData.size()) && (state != done); )
    {
        size_t bytesRead = 0;
        switch (state)
        {
            case readingLines:
            {
                nx::ConstBufferRefType readLine;
                if (lineSplitter.parseByLines(
                        nx::ConstBufferRefType(testData.data() + pos, 1),
                        &readLine,
                        &bytesRead))
                {
                    if (readLine.empty())
                        state = readingTrailingLineEnding;
                    else
                        lines.push_back(nx::Buffer(readLine));
                }
                break;
            }

            case readingTrailingLineEnding:
            {
                lineSplitter.finishCurrentLineEnding(
                    nx::ConstBufferRefType(testData.data() + pos, 1),
                    &bytesRead);
                state = done;
                break;
            }

            default:
            {
                FAIL();
            }
        }
        pos += bytesRead;
    }

    ASSERT_EQ(2U, lines.size());
    ASSERT_EQ("line1", lines[0]);
    ASSERT_EQ("line2", lines[1]);
    nx::Buffer msgBody = testData.substr(pos);
    ASSERT_EQ(
        "xxxxxxxx"
        "xxxxxxxx"
        "xxxxxxxx",
        msgBody);
}

TEST_F(LineSplitter, common)
{
    static const nx::Buffer testData =
        "line1\r\n"
        "line2\r\n"
        "line3";

    nx::network::http::LineSplitter lineSplitter;
    std::vector<nx::Buffer> lines;
    splitToLines(&lineSplitter, testData, &lines);
    ConstBufferRefType lineBuffer;
    size_t bytesRead = 0;
    ASSERT_TRUE(lineSplitter.parseByLines("\r\n", &lineBuffer, &bytesRead));
    ASSERT_EQ(2U, bytesRead);
    ASSERT_EQ("line3", lineBuffer);
}

} // namespace nx::network::http::test
