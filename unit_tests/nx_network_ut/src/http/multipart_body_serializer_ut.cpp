/**********************************************************
* May 12, 2016
* a.kolesnikov
***********************************************************/

#include <gtest/gtest.h>

#include <nx/utils/buffer_output_stream.h>
#include <nx/network/http/multipart_body_serializer.h>


namespace nx_http {

TEST(HttpMultipartBodySerializer, general)
{
    for (int i = 0; i < 2; ++i)
    {
        const bool closeMultipartBody = i == 1;

        auto bufferOutputStream = std::make_shared<nx::utils::bsf::BufferOutputStream>();
        MultipartBodySerializer serializer(
            "boundary",
            bufferOutputStream);

        nx::Buffer testData =
            "\r\n--boundary"
            "\r\n"
            "Content-Type: application/json\r\n"
            "\r\n"
            "1xxxxxxxxxxxxx2"
            "\r\n--boundary"
            "\r\n"
            "Content-Type: application/text\r\n"
            "\r\n"
            "3xxxxxx!!xxxxxxx4"
            "\r\n--boundary"
            "\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: 14\r\n"
            "\r\n"
            "5xxxxxxxxxxxx6";

        if (closeMultipartBody)
            testData += "\r\n--boundary--"; //terminating multipart body

        serializer.beginPart("application/json", nx_http::HttpHeaders(), BufferType());
        serializer.writeData("1xxxxxxxxxxxxx2");
        serializer.beginPart("application/text", nx_http::HttpHeaders(), "3xxxxxx!");
        serializer.writeData("!xxxxxxx4");
        serializer.writeBodyPart("text/html", nx_http::HttpHeaders(), "5xxxxxxxxxxxx6");

        if (closeMultipartBody)
            serializer.writeEpilogue();

        ASSERT_EQ(testData, bufferOutputStream->buffer());
    }
}

TEST(HttpMultipartBodySerializer, onlyEpilogue)
{
    auto bufferOutputStream = std::make_shared<nx::utils::bsf::BufferOutputStream>();
    MultipartBodySerializer serializer(
        "boundary",
        bufferOutputStream);

    nx::Buffer testData;
    testData += "\r\n--boundary--"; //terminating multipart body

    serializer.writeEpilogue();

    ASSERT_EQ(testData, bufferOutputStream->buffer());
}

}   //namespace nx_http
