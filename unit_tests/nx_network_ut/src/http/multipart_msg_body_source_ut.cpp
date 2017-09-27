#include <thread>

#include <gtest/gtest.h>

#include <nx/network/http/multipart_msg_body_source.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/byte_stream/buffer_output_stream.h>

namespace nx_http {

class HttpMultipartMessageBodySourceTest:
    public ::testing::Test
{
public:
    HttpMultipartMessageBodySourceTest():
        m_eofReported(false)
    {
    }

    ~HttpMultipartMessageBodySourceTest()
    {
        if (m_msgBodySource)
            m_msgBodySource->pleaseStopSync();
    }

protected:
    std::unique_ptr<MultipartMessageBodySource> m_msgBodySource;
    BufferType m_msgBody;
    bool m_eofReported;

    void initializeMsgBodySource()
    {
        using namespace std::placeholders;

        if (m_msgBodySource)
            m_msgBodySource->pleaseStopSync();
        m_msgBody.clear();
        m_eofReported = false;

        m_msgBodySource = std::make_unique<MultipartMessageBodySource>("boundary");
        m_msgBodySource->readAsync(
            std::bind(&HttpMultipartMessageBodySourceTest::onSomeBodyBytesRead, this, _1, _2));
    }

    void onSomeBodyBytesRead(SystemError::ErrorCode errorCode, BufferType buffer)
    {
        ASSERT_EQ(SystemError::noError, errorCode);

        if (buffer.isEmpty())
        {
            m_eofReported = true;
            return;
        }

        using namespace std::placeholders;

        m_msgBody += std::move(buffer);
        m_msgBodySource->readAsync(
            std::bind(&HttpMultipartMessageBodySourceTest::onSomeBodyBytesRead, this, _1, _2));
    }

    void thenSerializedMessageBodyIsEqualTo(BufferType expectedBody)
    {
        while (expectedBody.startsWith(m_msgBody))
        {
            if (m_msgBody == expectedBody)
                return;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        ASSERT_EQ(expectedBody, m_msgBody);
    }

    void thenEndOfStreamIsReported()
    {
        while (!m_eofReported)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
};

TEST_F(HttpMultipartMessageBodySourceTest, general)
{
    for (int i = 0; i < 2; ++i)
    {
        const bool closeMultipartBody = i == 1;

        initializeMsgBodySource();

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

        m_msgBodySource->serializer()->beginPart("application/json", nx_http::HttpHeaders(), BufferType());
        m_msgBodySource->serializer()->writeData("1xxxxxxxxxxxxx2");
        m_msgBodySource->serializer()->beginPart("application/text", nx_http::HttpHeaders(), "3xxxxxx!");
        m_msgBodySource->serializer()->writeData("!xxxxxxx4");
        m_msgBodySource->serializer()->writeBodyPart("text/html", nx_http::HttpHeaders(), "5xxxxxxxxxxxx6");

        if (closeMultipartBody)
            m_msgBodySource->serializer()->writeEpilogue();

        thenSerializedMessageBodyIsEqualTo(testData);
        if (closeMultipartBody)
            thenEndOfStreamIsReported();
    }
}

TEST_F(HttpMultipartMessageBodySourceTest, onlyEpilogue)
{
    initializeMsgBodySource();

    nx::Buffer testData;
    testData += "\r\n--boundary--"; //terminating multipart body

    m_msgBodySource->serializer()->writeEpilogue();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ASSERT_EQ(testData, m_msgBody);
    ASSERT_TRUE(m_eofReported);
}

}   //namespace nx_http
