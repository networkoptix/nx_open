#include <gtest/gtest.h>

#include <nx/network/connection_server/simple_message_server.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/stream_proxy.h>
#include <nx/network/stream_server_socket_to_acceptor_wrapper.h>
#include <nx/network/system_socket.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/std/optional.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx::network::test {

class BrokenAcceptor:
    public AbstractStreamSocketAcceptor
{
public:
    BrokenAcceptor(nx::utils::SyncQueue<int>* acceptRequestQueue):
        m_acceptRequestQueue(acceptRequestQueue)
    {
    }

    virtual void acceptAsync(AcceptCompletionHandler handler) override
    {
        m_acceptRequestQueue->push(0);
        post(
            [handler = std::move(handler)]()
            { handler(SystemError::invalidData, nullptr); });
    }

    virtual void cancelIOSync() override
    {
    }

private:
    nx::utils::SyncQueue<int>* m_acceptRequestQueue = nullptr;
};

//-------------------------------------------------------------------------------------------------

static constexpr char kTestHandlerPath[] = "/StreamProxyPool/test";

class StreamProxyPool:
    public ::testing::Test
{
public:
    StreamProxyPool():
        m_clientMessage("chisti-chisti"),
        m_serverMessage("raz-raz-raz")
    {
    }

    ~StreamProxyPool()
    {
        if (m_clientSocket)
            m_clientSocket->pleaseStopSync();
    }

protected:
    std::string m_clientMessage;
    std::string m_serverMessage;
    network::StreamProxyPool m_proxy;

    void givenListeningPingPongServer()
    {
        DestinationContext destinationContext;
        destinationContext.server = std::make_unique<server::SimpleMessageServer>(
            /*sslRequired*/ false,
            nx::network::NatTraversalSupport::disabled);
        destinationContext.server->setRequest(m_clientMessage.c_str());
        destinationContext.server->setResponse(m_serverMessage.c_str());
        ASSERT_TRUE(destinationContext.server->bind(SocketAddress::anyPrivateAddressV4));
        ASSERT_TRUE(destinationContext.server->listen());
        m_destinationServers.push_back(std::move(destinationContext));

        auto proxyServer = std::make_unique<network::TCPServerSocket>(AF_INET);
        ASSERT_TRUE(proxyServer->setNonBlockingMode(true));
        ASSERT_TRUE(proxyServer->bind(network::SocketAddress::anyPrivateAddressV4));
        ASSERT_TRUE(proxyServer->listen());
        m_destinationServers.back().proxyEndpoint = proxyServer->getLocalAddress();

        m_destinationServers.back().proxyId = m_proxy.addProxy(
            std::make_unique<StreamServerSocketToAcceptorWrapper>(std::move(proxyServer)),
            m_destinationServers.back().server->address());
    }

    void givenWorkingProxy()
    {
        givenListeningPingPongServer();
    }

    void givenProxyWithBrokenAcceptor()
    {
        m_proxy.addProxy(
            std::make_unique<BrokenAcceptor>(&m_acceptRequestQueue),
            SocketAddress(HostAddress::localhost, 12345));
    }

    void whenSendPingViaProxy()
    {
        m_expectedDestinationIndex = 0;

        m_clientSocket = std::make_unique<TCPSocket>(AF_INET);
        if (!m_clientSocket->connect(
                m_destinationServers[m_expectedDestinationIndex].proxyEndpoint,
                kNoTimeout))
        {
            saveRequestResult(SystemError::getLastOSErrorCode(), (std::size_t)-1);
            return;
        }
        ASSERT_EQ(
            m_clientMessage.size(),
            m_clientSocket->send(m_clientMessage.data(), m_clientMessage.size()));

        ASSERT_TRUE(m_clientSocket->setNonBlockingMode(true));
        m_readBuffer.clear();
        m_readBuffer.reserve(4*1024);
        m_clientSocket->readSomeAsync(
            &m_readBuffer,
            [this](auto&&... args) { saveRequestResult(std::move(args)...); });
    }

    void whenReassignToAnotherServer()
    {
        ASSERT_EQ(1U, m_destinationServers.size());
        givenListeningPingPongServer();

        m_proxy.setProxyDestination(
            m_destinationServers[0].proxyId,
            m_destinationServers[1].server->address());
    }

    void whenStopProxy()
    {
        m_proxy.stopProxy(m_destinationServers.front().proxyId);
    }

    void whenStopDestinationServer()
    {
        m_destinationServers.front().server.reset();
    }

    void thenPongIsReceived()
    {
        m_prevResponse = m_requestResults.pop();

        ASSERT_EQ(SystemError::noError, m_prevResponse->osResultCode);
        ASSERT_EQ(m_serverMessage, m_prevResponse->response);
    }

    void thenTrafficProxiedToAnotherServer()
    {
        whenSendPingViaProxy();

        m_expectedDestinationIndex = 1;

        thenPongIsReceived();
    }

    void thenProxyEndpointIsNotAvailable()
    {
        whenSendPingViaProxy();

        thenRequestHasFailed();
        andEndpointIsNotAvailable();
    }

    void thenRequestHasFailed()
    {
        m_prevResponse = m_requestResults.pop();

        // Connection can be closed gracefully without transferring any data. This is a failure too.
        ASSERT_TRUE(
            m_prevResponse->osResultCode != SystemError::noError ||
            m_prevResponse->response.empty());
    }

    void andEndpointIsNotAvailable()
    {
        ASSERT_NE(SystemError::noError, m_prevResponse->osResultCode);
    }

    void assertAcceptIsRetriedAfterFailure()
    {
        for (int i = 0; i < 2; ++i)
            m_acceptRequestQueue.pop();
    }

private:
    struct DestinationContext
    {
        std::unique_ptr<server::SimpleMessageServer> server;
        SocketAddress proxyEndpoint;
        int proxyId = -1;
    };

    struct RequestResult
    {
        SystemError::ErrorCode osResultCode = SystemError::noError;
        std::string response;
    };

    std::vector<DestinationContext> m_destinationServers;
    std::unique_ptr<TCPSocket> m_clientSocket;
    nx::utils::SyncQueue<RequestResult> m_requestResults;
    std::optional<RequestResult> m_prevResponse;
    int m_expectedDestinationIndex = -1;
    nx::utils::SyncQueue<int> m_acceptRequestQueue;
    nx::Buffer m_readBuffer;

    void saveRequestResult(
        SystemError::ErrorCode systemErrorCode,
        std::size_t bytesRead)
    {
        m_requestResults.push({
            systemErrorCode,
            systemErrorCode == SystemError::noError
                ? std::string(m_readBuffer.constData(), bytesRead)
                : std::string()});
    }
};

TEST_F(StreamProxyPool, data_is_delivered_to_the_target_server)
{
    givenListeningPingPongServer();
    whenSendPingViaProxy();
    thenPongIsReceived();
}

TEST_F(StreamProxyPool, change_proxy_destination)
{
    givenWorkingProxy();
    whenReassignToAnotherServer();
    thenTrafficProxiedToAnotherServer();
}

TEST_F(StreamProxyPool, stop_proxy)
{
    givenWorkingProxy();
    whenStopProxy();
    thenProxyEndpointIsNotAvailable();
}

TEST_F(StreamProxyPool, incoming_connection_is_closed_if_destination_is_not_available)
{
    givenWorkingProxy();

    whenStopDestinationServer();
    whenSendPingViaProxy();

    thenRequestHasFailed();
}

TEST_F(StreamProxyPool, accept_is_retried_after_failure)
{
    givenProxyWithBrokenAcceptor();
    assertAcceptIsRetriedAfterFailure();
}

//-------------------------------------------------------------------------------------------------

class CustomFuncOutputConverter:
    public nx::utils::bstream::AbstractOutputConverter
{
public:
    CustomFuncOutputConverter(
        std::function<void(void* /*data*/, std::size_t /*count*/)> func)
        :
        m_func(std::move(func))
    {
    }

    virtual int write(const void* data, size_t count) override
    {
        std::string convertedData(static_cast<const char*>(data), count);
        m_func(convertedData.data(), convertedData.size());
        return m_outputStream->write(convertedData.data(), convertedData.size());
    }

private:
    std::function<void(void* /*data*/, std::size_t /*count*/)> m_func;
};

static void convert(void* data, std::size_t count)
{
    char* charData = static_cast<char*>(data);
    for (std::size_t i = 0; i < count; ++i)
        charData[i] ^= 'z';
}

class StreamProxyPoolWithConverter:
    public StreamProxyPool
{
    using base_type = StreamProxyPool;

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        m_proxy.setDownStreamConverterFactory(
            [this]() { return std::make_unique<CustomFuncOutputConverter>(&convert); });
        m_proxy.setUpStreamConverterFactory(
            [this]() { return std::make_unique<CustomFuncOutputConverter>(&convert); });

        givenListeningPingPongServer();

        convert(m_clientMessage.data(), m_clientMessage.size());
        convert(m_serverMessage.data(), m_serverMessage.size());
    }
};

TEST_F(StreamProxyPoolWithConverter, converts_stream)
{
    // If outgoing stream would have not been converted, server would not respond.
    // If incoming stream would have not been converted,
    // server response would have not been equal to m_serverMessage.

    whenSendPingViaProxy();
    thenPongIsReceived();
}

} // namespace nx::network::test
