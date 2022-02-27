// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <deque>

#include <gtest/gtest.h>

#include <nx/network/http/multipart_content_parser.h>
#include <nx/utils/random.h>

#include <nx/utils/byte_stream/custom_output_stream.h>

namespace nx::network::http::test {

TEST(HttpMultipartContentParser, genericTest)
{
    const nx::Buffer frame1 =
        "1xxxxxxxxxxxxxxx\rxxxxxxxx\nxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxx\r\nxxxxxxxxxxxxxxxx2";
    const nx::Buffer frame2 =
        "3xxxxxxxxxxxxx\nxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxx\r\r\rx\rxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxx\n\n\n\nxxx\r\n\r\nxxxxxxxxx4";
    const nx::Buffer frame3 = "";
    const nx::Buffer frame4 =
        "5xxxxxxxxxx\r\n\r\n\r\n\r\n\r\r\r\r\r\n\n\n\nyyyyyyyyyyyyyy6";

    for (int i = 0; i < 2; ++i)
    {
        const bool closeContent = i > 0;

        nx::Buffer testData =
            "\r\n--fbdr"
            "\r\n"
            "Content-Type: image/jpeg\r\n"
            "\r\n"
            + frame1 +
            "\r\n--fbdr"
            "\r\n"
            "Content-Type: image/jpeg\r\n"
            "\r\n"
            + frame2 +
            "\r\n--fbdr"
            "\r\n"
            "Content-Length: " + std::to_string(frame3.size()) + "\r\n"
            "Content-Type: image/jpeg\r\n"
            "\r\n"
            + frame3 +
            "\r\n--fbdr"
            "\r\n"
            "Content-Type: image/jpeg\r\n"
            "\r\n"
            + frame4;
        if (closeContent)
            testData += "\r\n--fbdr--"; //terminating multipart body

        for (std::size_t dataStep = 1; dataStep < testData.size(); ++dataStep)
        {
            nx::network::http::MultipartContentParser parser;
            parser.setContentType("multipart/x-mixed-replace;boundary=fbdr");

            std::deque<nx::Buffer> frames;
            std::deque<nx::network::http::HttpHeaders> headers;
            auto decodedFramesProcessor = [&frames, &headers, &parser](const ConstBufferRefType& data) {
                frames.push_back(nx::Buffer(data));
                headers.push_back(parser.prevFrameHeaders());
            };

            parser.setNextFilter(
                std::make_shared<nx::utils::bstream::CustomOutputStream<decltype(decodedFramesProcessor)> >(
                    decodedFramesProcessor));

#if 0       //TODO #akolesnikov fix
            for (size_t pos = 0; pos < testData.size(); pos += dataStep)
                parser.processData(ConstBufferRefType(
                    testData,
                    pos,
                    std::min<std::size_t>(dataStep, testData.size() - pos)));
#else
            parser.processData(testData);
#endif
            parser.flush();

            if (closeContent)
            {
                ASSERT_TRUE(parser.eof());
                ASSERT_EQ(5U, frames.size());
                ASSERT_EQ(5U, headers.size());
                ASSERT_TRUE(frames[4].empty());
            }
            else
            {
                ASSERT_FALSE(parser.eof());
                ASSERT_EQ(4U, frames.size());
                ASSERT_EQ(4U, headers.size());
            }
            ASSERT_EQ(frames[0], frame1);
            ASSERT_EQ(nx::network::http::getHeaderValue(headers[0], "Content-Type"), "image/jpeg");
            ASSERT_EQ(frames[1], frame2);
            ASSERT_EQ(nx::network::http::getHeaderValue(headers[1], "Content-Type"), "image/jpeg");
            ASSERT_EQ(frames[2], frame3);
            ASSERT_EQ(nx::network::http::getHeaderValue(headers[2], "Content-Type"), "image/jpeg");
            ASSERT_EQ(nx::network::http::getHeaderValue(headers[2], "Content-Length"), std::to_string(frame3.size()));
            ASSERT_EQ(frames[3], frame4);
            ASSERT_EQ(nx::network::http::getHeaderValue(headers[3], "Content-Type"), "image/jpeg");
        }
    }
}

TEST(HttpMultipartContentParser, onlySizedData)
{
    const nx::Buffer frame1 =
        "1xxxxxxxxxxxxxxx\rxxxxxxxx\nxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxx\r\nxxxxxxxxxxxxxxxx2";
    const nx::Buffer frame2 =
        "3xxxxxxxxxxxxx\nxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxx\r\r\rx\rxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxx\n\n\n\nxxx\r\n\r\nxxxxxxxxx4";
    const nx::Buffer frame3 = "";
    const nx::Buffer frame4 =
        "5xxxxxxxxxx\r\n\r\n\r\n\r\n\r\r\r\r\r\n\n\n\nyyyyyyyyyyyyyy6";

    for (int i = 0; i < 2; ++i)
    {
        const bool closeContent = i > 0;

        nx::Buffer testData =
            "\r\n--fbdr"
            "\r\n"
            "Content-Length: " + std::to_string(frame1.size()) + "\r\n"
            "Content-Type: image/jpeg\r\n"
            "\r\n"
            + frame1 +
            "\r\n--fbdr"
            "\r\n"
            "Content-Length: " + std::to_string(frame2.size()) + "\r\n"
            "Content-Type: image/jpeg\r\n"
            "\r\n"
            + frame2 +
            "\r\n--fbdr"
            "\r\n"
            "Content-Length: " + std::to_string(frame3.size()) + "\r\n"
            "Content-Type: image/jpeg\r\n"
            "\r\n"
            + frame3 +
            "\r\n--fbdr"
            "\r\n"
            "Content-Length: " + std::to_string(frame4.size()) + "\r\n"
            "Content-Type: image/jpeg\r\n"
            "\r\n"
            + frame4;
        if (closeContent)
            testData += "\r\n--fbdr--"; //terminating multipart body

        for (std::size_t dataStep = 1; dataStep < testData.size(); ++dataStep)
        {
            nx::network::http::MultipartContentParser parser;
            parser.setContentType("multipart/x-mixed-replace;boundary=fbdr");

            std::deque<nx::Buffer> frames;
            auto decodedFramesProcessor = [&frames](const ConstBufferRefType& data) {
                frames.push_back(nx::Buffer(data));
            };

            parser.setNextFilter(
                std::make_shared<nx::utils::bstream::CustomOutputStream<decltype(decodedFramesProcessor)> >(
                    decodedFramesProcessor));

            for (std::size_t pos = 0; pos < testData.size(); pos += dataStep)
            {
                parser.processData(ConstBufferRefType(
                    testData.data() + pos,
                    std::min<std::size_t>(dataStep, testData.size() - pos)));
            }

            parser.flush();

            if (closeContent)
            {
                ASSERT_TRUE(parser.eof());
                ASSERT_EQ(5U, frames.size());
                ASSERT_TRUE(frames[4].empty());
            }
            else
            {
                ASSERT_FALSE(parser.eof());
                ASSERT_EQ(4U, frames.size());
            }
            ASSERT_EQ(frame1, frames[0]);
            ASSERT_EQ(frame2, frames[1]);
            ASSERT_EQ(frame3, frames[2]);
            ASSERT_EQ(frame4, frames[3]);
        }
    }
    //TODO #akolesnikov test with Content-Length specified
}

TEST(HttpMultipartContentParser, unSizedDataSimple)
{
    const nx::Buffer frame1 =
        "a\rb";
    const nx::Buffer frame2 =
        "c\rd";

    const nx::Buffer testData =
        "\r\n--fbdr"    //boundary
        "\r\n"
        "Content-Type: image/jpeg\r\n"
        "\r\n"
        + frame1 +
        "\r\n--fbdr"
        "\r\n"
        "Content-Type: image/jpeg\r\n"
        "\r\n"
        + frame2/* +
        "\r\n"*/;

        //for( int dataStep = 1; dataStep < testData.size(); ++dataStep )
    for (std::size_t dataStep = 1; dataStep < testData.size(); ++dataStep)
    {
        nx::network::http::MultipartContentParser parser;
        parser.setContentType("multipart/x-mixed-replace;boundary=fbdr");

        std::deque<nx::Buffer> frames;
        auto decodedFramesProcessor = [&frames](const ConstBufferRefType& data) {
            frames.push_back(nx::Buffer(data));
        };

        parser.setNextFilter(
            std::make_shared<nx::utils::bstream::CustomOutputStream<decltype(decodedFramesProcessor)> >(
                decodedFramesProcessor));

        for (std::size_t pos = 0; pos < testData.size(); pos += dataStep)
        {
            parser.processData(ConstBufferRefType(
                testData.data() + pos,
                std::min<std::size_t>(dataStep, testData.size() - pos)));
        }
        parser.flush();

        ASSERT_EQ(2U, frames.size());
        ASSERT_EQ(frames[0], frame1);
        ASSERT_EQ(frames[1], frame2);
    }
}

TEST(HttpMultipartContentParser, unSizedData)
{
    static const size_t FRAMES_COUNT = nx::utils::random::number<size_t>(3, 7);
    static const size_t FRAME_SIZE_MIN = 1024;
    static const size_t FRAME_SIZE_MAX = 256 * 1024;

    const size_t DATA_STEPS[] = { 1, 2, 3, 20, 48, 4 * 1024, 16 * 1024 };

    {
        //generating test frames
        std::vector<nx::Buffer> testFrames(FRAMES_COUNT);
        for (nx::Buffer& testFrame : testFrames)
        {
            testFrame.resize(nx::utils::random::number<size_t>(FRAME_SIZE_MIN, FRAME_SIZE_MAX));

            std::generate(
                testFrame.data(),
                testFrame.data() + testFrame.size(),
                [] { return nx::utils::random::number<int>(0, std::numeric_limits<char>::max()); });
            ASSERT_EQ(testFrame.npos, testFrame.find("--fbdr"));
        }

        //test data
        nx::Buffer testData;
        for (const nx::Buffer& testFrame : testFrames)
        {
            testData +=
                "\r\n--fbdr"    //delimiter
                "\r\n"
                "Content-Type: image/jpeg\r\n"
                "\r\n"
                + testFrame;
        }

        for (const auto dataStep : DATA_STEPS)
        {
            nx::network::http::MultipartContentParser parser;
            parser.setContentType("multipart/x-mixed-replace;boundary=fbdr");

            std::deque<nx::Buffer> readFrames;
            auto decodedFramesProcessor = [&readFrames](const ConstBufferRefType& data) {
                readFrames.push_back(nx::Buffer(data));
            };

            parser.setNextFilter(
                std::make_shared<nx::utils::bstream::CustomOutputStream<decltype(decodedFramesProcessor)> >(
                    decodedFramesProcessor));

            for (size_t pos = 0; pos < (size_t)testData.size(); pos += dataStep)
            {
                parser.processData(ConstBufferRefType(
                    testData.data() + pos,
                    std::min<std::size_t>(dataStep, testData.size() - pos)));
            }
            parser.flush();

            ASSERT_EQ(readFrames.size(), testFrames.size());
            for (size_t i = 0; i < readFrames.size(); ++i)
            {
                ASSERT_EQ(readFrames[i], testFrames[i]);
            }
        }
    }
}

TEST(HttpMultipartContentParser, epilogueOnly)
{
    nx::Buffer testData = "\r\n--fbdr--"; //terminating multipart body

    for (std::size_t dataStep = 1; dataStep < testData.size(); ++dataStep)
    {
        nx::network::http::MultipartContentParser parser;
        parser.setContentType("multipart/x-mixed-replace;boundary=fbdr");

        std::deque<nx::Buffer> frames;
        auto decodedFramesProcessor = [&frames](const ConstBufferRefType& data) {
            frames.push_back(nx::Buffer(data));
        };

        parser.setNextFilter(
            std::make_shared<nx::utils::bstream::CustomOutputStream<decltype(decodedFramesProcessor)> >(
                decodedFramesProcessor));

        parser.processData(testData);

        ASSERT_TRUE(parser.eof());
        ASSERT_EQ(1U, frames.size());
        ASSERT_TRUE(frames[0].empty());
    }
}

} // namespace nx::network::http::test
