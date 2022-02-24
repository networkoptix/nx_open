// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/network/http/writable_message_body.h>
#include <nx/utils/random.h>
#include <nx/utils/string.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx::network::http::test {

class HttpWritableMessageBody:
    public ::testing::Test
{
public:
    ~HttpWritableMessageBody()
    {
        if (m_messageBody)
            m_messageBody->pleaseStopSync();
    }

protected:
    virtual void SetUp() override
    {
        m_messageBody = std::make_unique<http::WritableMessageBody>(
            "text/plain",
            std::nullopt);
    }

    void writeRandomData()
    {
        auto data = nx::utils::generateRandomName(1024);
        m_messageBody->writeBodyData(data);

        m_dataWritten.append(data);
    }

    void writeEof(SystemError::ErrorCode resultCode)
    {
        m_messageBody->writeEof(resultCode);
    }

    void startReadingMessageBodyAsync()
    {
        m_messageBody->readAsync(
            [this](auto resultCode, auto buffer)
            {
                m_readQueue.push(std::make_tuple(resultCode, std::move(buffer)));
                startReadingMessageBodyAsync();
            });
    }

    void readSync()
    {
        std::promise<std::tuple<SystemError::ErrorCode, nx::Buffer>> done;
        m_messageBody->readAsync(
            [&done](SystemError::ErrorCode resultCode, nx::Buffer data)
            {
                done.set_value(std::make_tuple(resultCode, std::move(data)));
            });
        m_readQueue.push(done.get_future().get());
    }

    void cancelRead()
    {
        m_messageBody->cancelRead();
    }

    void assertTheWholeDataIsReported()
    {
        const auto result = m_readQueue.pop();
        ASSERT_EQ(SystemError::noError, std::get<0>(result));
        ASSERT_EQ(m_dataWritten, std::get<1>(result));
    }

    void assertEofIsReported(SystemError::ErrorCode resultCode)
    {
        const auto result = m_readQueue.pop();
        ASSERT_EQ(resultCode, std::get<0>(result));
        ASSERT_TRUE(std::get<1>(result).empty());
    }

    void assertNoDataIsReported()
    {
        ASSERT_FALSE(m_readQueue.pop(std::chrono::milliseconds(100)));
    }

private:
    std::unique_ptr<http::WritableMessageBody> m_messageBody;
    nx::Buffer m_dataWritten;
    nx::utils::SyncQueue<std::tuple<SystemError::ErrorCode, nx::Buffer>> m_readQueue;
};

TEST_F(HttpWritableMessageBody, the_whole_data_is_delivered_before_eof)
{
    writeRandomData();
    writeEof(SystemError::connectionReset);

    readSync();
    assertTheWholeDataIsReported();

    readSync();
    assertEofIsReported(SystemError::connectionReset);
}

TEST_F(HttpWritableMessageBody, the_whole_data_is_delivered_before_eof_2)
{
    startReadingMessageBodyAsync();

    writeRandomData();
    writeEof(SystemError::connectionReset);

    assertTheWholeDataIsReported();
    assertEofIsReported(SystemError::connectionReset);
}

TEST_F(HttpWritableMessageBody, cancel_read)
{
    startReadingMessageBodyAsync();
    cancelRead();

    writeRandomData();
    assertNoDataIsReported();
}

} // namespace nx::network::http::test
