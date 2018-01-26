#include <gtest/gtest.h>

#include <nx/network/cloud/abstract_cloud_system_credentials_provider.h>
#include <nx/network/cloud/mediator_address_publisher.h>
#include <nx/utils/test_support/sync_queue.h>

namespace nx {
namespace network {
namespace cloud {
namespace test {

class AsyncStunClient:
    public stun::AbstractAsyncClient
{
public:
    virtual ~AsyncStunClient() override
    {
        if (isInSelfAioThread())
            stopWhileInAioThread();
    }

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread)
    {
        stun::AbstractAsyncClient::bindToAioThread(aioThread);
    }

    virtual void connect(const QUrl& /*url*/, ConnectHandler handler = nullptr) override
    {
        if (handler)
            post([handler = std::move(handler)]() { handler(SystemError::noError); });
    }

    virtual bool setIndicationHandler(
        int /*method*/, IndicationHandler /*handler*/, void* /*client*/ = 0) override
    {
        return true;
    }

    virtual void addOnReconnectedHandler(ReconnectHandler handler, void* /*client*/ = 0) override
    {
        post(
            [this, handler = std::move(handler)]()
            {
                m_reconnectHandlers.push_back(std::move(handler));
            });
    }

    virtual void setOnConnectionClosedHandler(
        OnConnectionClosedHandler /*onConnectionClosedHandler*/) override
    {
    }

    virtual void sendRequest(
        stun::Message /*request*/,
        RequestHandler handler,
        void* /*client*/ = 0) override
    {
        post(
            [this, handler = std::move(handler)]()
            {
                handler(SystemError::connectionReset, stun::Message());
                reportReconnect();
            });
    }

    virtual bool addConnectionTimer(
        std::chrono::milliseconds /*period*/, TimerHandler /*handler*/, void* /*client*/)
    {
        return true;
    }

    virtual SocketAddress localAddress() const override
    {
        return SocketAddress();
    }

    virtual SocketAddress remoteAddress() const override
    {
        return SocketAddress();
    }

    virtual void closeConnection(SystemError::ErrorCode /*errorCode*/) override
    {
    }

    virtual void cancelHandlers(
        void* /*client*/,
        utils::MoveOnlyFunc<void()> /*handler*/) override
    {
    }

    virtual void setKeepAliveOptions(KeepAliveOptions /*options*/) override
    {
    }

private:
    std::list<ReconnectHandler> m_reconnectHandlers;

    void reportReconnect()
    {
        for (auto& handler: m_reconnectHandlers)
            handler();
    }
};

class TestCloudCredentialsProvider:
    public hpm::api::AbstractCloudSystemCredentialsProvider
{
public:
    TestCloudCredentialsProvider()
    {
        m_cloudCredentials.systemId = QnUuid::createUuid().toSimpleByteArray();
        m_cloudCredentials.serverId = QnUuid::createUuid().toSimpleByteArray();
        m_cloudCredentials.key = QnUuid::createUuid().toSimpleByteArray();
    }

    virtual boost::optional<hpm::api::SystemCredentials> getSystemCredentials() const override
    {
        return m_cloudCredentials;
    }

private:
    hpm::api::SystemCredentials m_cloudCredentials;
};

class MediatorAddressPublisher:
    public ::testing::Test
{
public:
    static constexpr int requestCount = 3;

    MediatorAddressPublisher():
        m_stunClient(std::make_shared<AsyncStunClient>())
    {
    }

    ~MediatorAddressPublisher()
    {
        if (m_publisher)
            m_publisher->pleaseStopSync();
        if (m_stunClient)
            m_stunClient->pleaseStopSync();
    }

protected:
    void givenMediatorClientThatReconnectsOnEachRequest()
    {
        m_publisher = std::make_unique<cloud::MediatorAddressPublisher>(
            std::make_unique<hpm::api::MediatorServerTcpConnection>(
                m_stunClient,
                &m_cloudCredentialsProvider));
    }

    void whenIssuedBindRequest()
    {
        using namespace std::placeholders;

        std::list<SocketAddress> addresses{ SocketAddress::anyPrivateAddress };
        for (int i = 0; i < requestCount; ++i)
        {
            m_publisher->updateAddresses(
                addresses,
                std::bind(&MediatorAddressPublisher::onUpdateAddressesCompleted, this, _1));
        }
    }

    void thenErrorShouldBeReported()
    {
        for (int i = 0; i < requestCount; ++i)
        {
            ASSERT_NE(hpm::api::ResultCode::ok, m_updateAddressResults.pop());
        }
    }

private:
    TestCloudCredentialsProvider m_cloudCredentialsProvider;
    std::shared_ptr<AsyncStunClient> m_stunClient;
    std::unique_ptr<cloud::MediatorAddressPublisher> m_publisher;
    utils::SyncQueue<nx::hpm::api::ResultCode> m_updateAddressResults;

    void onUpdateAddressesCompleted(hpm::api::ResultCode resultCode)
    {
        m_updateAddressResults.push(resultCode);
    }
};

TEST_F(MediatorAddressPublisher, completion_handler_is_triggered_after_reconnect)
{
    givenMediatorClientThatReconnectsOnEachRequest();
    whenIssuedBindRequest();
    thenErrorShouldBeReported();
}

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
