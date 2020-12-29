#include <gtest/gtest.h>

#include <nx/utils/byte_stream/buffer_output_stream.h>
#include <nx/network/http/multipart_body_serializer.h>

namespace nx {
namespace network {
namespace http {

TEST(HttpMultipartBodySerializer, general)
{
    for (int i = 0; i < 2; ++i)
    {
        const bool closeMultipartBody = i == 1;

        auto bufferOutputStream = std::make_shared<nx::utils::bstream::BufferOutputStream>();
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

        serializer.beginPart("application/json", nx::network::http::HttpHeaders(), BufferType());
        serializer.writeData("1xxxxxxxxxxxxx2");
        serializer.beginPart("application/text", nx::network::http::HttpHeaders(), "3xxxxxx!");
        serializer.writeData("!xxxxxxx4");
        serializer.writeBodyPart("text/html", nx::network::http::HttpHeaders(), "5xxxxxxxxxxxx6");

        if (closeMultipartBody)
            serializer.writeEpilogue();

        ASSERT_EQ(testData, bufferOutputStream->buffer());
    }
}

TEST(HttpMultipartBodySerializer, onlyEpilogue)
{
    auto bufferOutputStream = std::make_shared<nx::utils::bstream::BufferOutputStream>();
    MultipartBodySerializer serializer(
        "boundary",
        bufferOutputStream);

    nx::Buffer testData;
    testData += "\r\n--boundary--"; //terminating multipart body

    serializer.writeEpilogue();

    ASSERT_EQ(testData, bufferOutputStream->buffer());
}

} // namespace nx
} // namespace network
} // namespace http
