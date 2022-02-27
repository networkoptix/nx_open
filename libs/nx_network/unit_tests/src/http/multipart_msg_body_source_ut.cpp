// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <thread>

#include <gtest/gtest.h>

#include <nx/network/http/multipart_msg_body_source.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/byte_stream/buffer_output_stream.h>

namespace nx::network::http::test {

class HttpMultipartMessageBodySource:
    public ::testing::Test
{
public:
    ~HttpMultipartMessageBodySource()
    {
        if (m_msgBodySource)
            m_msgBodySource->pleaseStopSync();
    }

protected:
    std::unique_ptr<MultipartMessageBodySource> m_msgBodySource;
    nx::Buffer m_msgBody;
    bool m_eofReported = false;
    nx::Buffer m_expectedData;

    void whenWriteEpilogue()
    {
        if (!m_msgBodySource)
            initializeMsgBodySource();

        m_expectedData += "\r\n--boundary--"; //terminating multipart body

        m_msgBodySource->serializer()->writeEpilogue();
    }

    void thenSerializedDataMatchesExpectations()
    {
        thenSerializedMessageBodyIsEqualTo(m_expectedData);
    }

    void thenSerializedMessageBodyIsEqualTo(nx::Buffer expectedBody)
    {
        NX_MUTEX_LOCKER lock(&m_mutex);

        while (expectedBody.starts_with(m_msgBody))
        {
            if (m_msgBody == expectedBody)
                return;

            lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            lock.relock();
        }

        ASSERT_EQ(expectedBody, m_msgBody);
    }

    void thenEndOfStreamIsReported()
    {
        while (!m_eofReported)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    void initializeMsgBodySource()
    {
        using namespace std::placeholders;

        if (m_msgBodySource)
            m_msgBodySource->pleaseStopSync();
        m_msgBody.clear();
        m_eofReported = false;

        m_msgBodySource = std::make_unique<MultipartMessageBodySource>("boundary");
        m_msgBodySource->readAsync(
            std::bind(&HttpMultipartMessageBodySource::onSomeBodyBytesRead, this, _1, _2));
    }

private:
    nx::Mutex m_mutex;

    void onSomeBodyBytesRead(SystemError::ErrorCode errorCode, nx::Buffer buffer)
    {
        using namespace std::placeholders;

        ASSERT_EQ(SystemError::noError, errorCode);

        if (buffer.empty())
        {
            m_eofReported = true;
            return;
        }

        {
            NX_MUTEX_LOCKER lock(&m_mutex);
            m_msgBody += std::move(buffer);
        }

        m_msgBodySource->readAsync(
            std::bind(&HttpMultipartMessageBodySource::onSomeBodyBytesRead, this, _1, _2));
    }
};

TEST_F(HttpMultipartMessageBodySource, general)
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

        m_msgBodySource->serializer()->beginPart("application/json", nx::network::http::HttpHeaders(), nx::Buffer());
        m_msgBodySource->serializer()->writeData("1xxxxxxxxxxxxx2");
        m_msgBodySource->serializer()->beginPart("application/text", nx::network::http::HttpHeaders(), "3xxxxxx!");
        m_msgBodySource->serializer()->writeData("!xxxxxxx4");
        m_msgBodySource->serializer()->writeBodyPart("text/html", nx::network::http::HttpHeaders(), "5xxxxxxxxxxxx6");

        if (closeMultipartBody)
            m_msgBodySource->serializer()->writeEpilogue();

        thenSerializedMessageBodyIsEqualTo(testData);
        if (closeMultipartBody)
            thenEndOfStreamIsReported();
    }
}

TEST_F(HttpMultipartMessageBodySource, onlyEpilogue)
{
    whenWriteEpilogue();
    thenSerializedDataMatchesExpectations();
    thenEndOfStreamIsReported();
}

} // namespace nx::network::http::test
