#include <vector>

#include <gtest/gtest.h>

#include <nx/network/http/line_splitter.h>

namespace {

void splitToLines(
    nx_http::LineSplitter* const lineSplitter,
    const nx::Buffer& sourceText,
    std::vector<nx::Buffer>* const lines)
{
    for (int pos = 0; pos < sourceText.size(); )
    {
        QnByteArrayConstRef readLine;
        size_t bytesRead = 0;
        if (lineSplitter->parseByLines(
            QnByteArrayConstRef(sourceText, pos),
            &readLine,
            &bytesRead))
            lines->push_back(readLine);
        pos += bytesRead;
    }
}

#if 0
void splitToLines(const nx::Buffer& sourceText, std::vector<nx::Buffer>* const lines)
{
    nx_http::LineSplitter lineSplitter;
    splitToLines(&lineSplitter, sourceText, lines);
}
#endif

}

#if 0
//TODO #ak uncomment and fix
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

TEST(LineSplitter, TrailingLFTest)
{
    static const nx::Buffer testData =
        "line1\r\n"
        "line2\r\n"
        "\r\n"
        "xxxxxxxx"
        "xxxxxxxx"
        "xxxxxxxx"
        ;

    nx_http::LineSplitter lineSplitter;
    std::vector<nx::Buffer> lines;
    enum State
    {
        readingLines,
        readingTrailingLineEnding,
        done
    }
    state = readingLines;
    int pos = 0;
    for (; (pos < testData.size()) && (state != done); )
    {
        size_t bytesRead = 0;
        switch (state)
        {
            case readingLines:
            {
                QnByteArrayConstRef readLine;
                if (lineSplitter.parseByLines(
                    QnByteArrayConstRef(testData, pos, 1),
                    &readLine,
                    &bytesRead))
                {
                    if (readLine.isEmpty())
                        state = readingTrailingLineEnding;
                    else
                        lines.push_back(readLine);
                }
                break;
            }
            case readingTrailingLineEnding:
            {
                lineSplitter.finishCurrentLineEnding(
                    QnByteArrayConstRef(testData, pos, 1),
                    &bytesRead);
                state = done;
                break;
            }
            default:
            {
                ASSERT_TRUE(false);
            }
        }
        pos += bytesRead;
    }

    ASSERT_EQ(2U, lines.size());
    ASSERT_EQ("line1", lines[0]);
    ASSERT_EQ("line2", lines[1]);
    nx::Buffer msgBody = testData.mid(pos);
    ASSERT_EQ(
        "xxxxxxxx"
        "xxxxxxxx"
        "xxxxxxxx",
        msgBody);
}

TEST(LineSplitter, common)
{
    static const nx::Buffer testData =
        "line1\r\n"
        "line2\r\n"
        "line3";

    nx_http::LineSplitter lineSplitter;
    std::vector<nx::Buffer> lines;
    splitToLines(&lineSplitter, testData, &lines);
    QnByteArrayConstRef lineBuffer;
    size_t bytesRead = 0;
    ASSERT_TRUE(lineSplitter.parseByLines(QByteArray("\r\n"), &lineBuffer, &bytesRead));
    ASSERT_EQ(2U, bytesRead);
    ASSERT_EQ("line3", lineBuffer);
}
