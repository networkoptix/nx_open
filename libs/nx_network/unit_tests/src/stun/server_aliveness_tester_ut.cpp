#include <gtest/gtest.h>

#include <nx/network/stun/async_client_delegate.h>
#include <nx/network/stun/server_aliveness_tester.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx::network::stun::test {

namespace {

class TestClient:
    public AsyncClientDelegate
{
    using base_type = AsyncClientDelegate;

public:
    TestClient(
        nx::utils::SyncQueue<int /*dummy*/>* sendRequestCalls,
        nx::utils::SyncQueue<int /*dummy*/>* cancelRequestCalls)
        :
        base_type(nullptr),
        m_sendRequestCalls(sendRequestCalls),
        m_cancelRequestCalls(cancelRequestCalls)
    {
    }

public:
    virtual void sendRequest(
        nx::network::stun::Message /*request*/,
        RequestHandler /*handler*/,
        void* /*client*/) override
    {
        m_sendRequestCalls->push(0);
    }

    virtual void cancelHandlers(
        void* /*client*/,
        utils::MoveOnlyFunc<void()> /*handler*/) override
    {
        m_cancelRequestCalls->push(0);
    }

private:
    nx::utils::SyncQueue<int /*dummy*/>* m_sendRequestCalls = nullptr;
    nx::utils::SyncQueue<int /*dummy*/>* m_cancelRequestCalls = nullptr;
};

} // namespace

//-------------------------------------------------------------------------------------------------

class ServerAlivenessTester:
    public ::testing::Test
{
public:
    ServerAlivenessTester():
        m_client(&m_sendRequestCalls, &m_cancelRequestCalls)
    {
        m_tester = std::make_unique<stun::ServerAlivenessTester>(
            KeepAliveOptions(std::chrono::milliseconds(1), std::chrono::milliseconds(1), 3),
            &m_client);
    }

    ~ServerAlivenessTester()
    {
        if (m_tester)
        {
            m_tester->pleaseStopSync();
            m_tester.reset();
        }

        m_client.pleaseStopSync();
    }

protected:
    void startTester()
    {
        m_tester->start([]() {});
    }

    void waitForTesterToSendRequest()
    {
        m_sendRequestCalls.pop();
    }

    void whenDestroyTester()
    {
        m_tester->pleaseStopSync();
        m_tester.reset();
    }

    void thenRequestIsCancelled()
    {
        ASSERT_TRUE(m_cancelRequestCalls.pop(std::chrono::milliseconds::zero()));
    }

private:
    nx::utils::SyncQueue<int /*dummy*/> m_sendRequestCalls;
    nx::utils::SyncQueue<int /*dummy*/> m_cancelRequestCalls;
    TestClient m_client;
    std::unique_ptr<stun::ServerAlivenessTester> m_tester;
};

TEST_F(ServerAlivenessTester, cancels_issued_requests_before_destruction)
{
    startTester();
    waitForTesterToSendRequest();

    whenDestroyTester();

    thenRequestIsCancelled();
}

} // namespace nx::network::stun::test
