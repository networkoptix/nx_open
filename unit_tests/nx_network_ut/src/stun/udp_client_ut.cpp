#include <condition_variable>
#include <mutex>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <nx/network/stun/message_dispatcher.h>
#include <nx/network/stun/udp_client.h>
#include <nx/network/stun/udp_server.h>
#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/string.h>
#include <nx/utils/test_support/test_options.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>
#include <utils/common/sync_call.h>

namespace nx {
namespace stun {
namespace test {

class UdpClient:
    public ::testing::Test
{
public:
    static const QByteArray kServerResponseNonce;

    UdpClient():
        m_messagesToIgnore(0),
        m_totalMessagesReceived(0)
    {
        addServer();
    }

    ~UdpClient()
    {
        for (const auto& serverCtx: m_udpServers)
            serverCtx->server->pleaseStopSync();
    }

    /**
     * Launches another server.
     */
    void addServer()
    {
        using namespace std::placeholders;

        auto ctx = std::make_unique<ServerContext>();
        ctx->server = std::make_unique<UdpServer>(&ctx->messageDispatcher);
        ASSERT_TRUE(ctx->server->bind(SocketAddress(HostAddress::localhost, 0)));
        ASSERT_TRUE(ctx->server->listen());
        ctx->messageDispatcher.registerDefaultRequestProcessor(
            std::bind(&UdpClient::onMessage, this, _1, _2));

        m_udpServers.emplace_back(std::move(ctx));
    }

    /**
     * @return endpoint of any server.
     */
    SocketAddress anyServerEndpoint() const
    {
        return SocketAddress(
            HostAddress::localhost,
            nx::utils::random::choice(m_udpServers)->server->address().port);
    }

    std::vector<SocketAddress> allServersEndpoints() const
    {
        std::vector<SocketAddress> endpoints;
        endpoints.reserve(m_udpServers.size());
        for (const auto& serverCtx: m_udpServers)
        {
            endpoints.push_back(
                SocketAddress(
                    HostAddress::localhost,
                    serverCtx->server->address().port));
        }

        return endpoints;
    }

    void ignoreNextMessage(size_t messagesToIgnore = 1)
    {
        m_messagesToIgnore += messagesToIgnore;
    }

    size_t totalMessagesReceived() const
    {
        return m_totalMessagesReceived;
    }

protected:
    struct ServerContext
    {
        MessageDispatcher messageDispatcher;
        std::unique_ptr<UdpServer> server;
    };

    ServerContext& server(std::size_t index)
    {
        return *m_udpServers[index];
    }

private:
    std::vector<std::unique_ptr<ServerContext>> m_udpServers;
    size_t m_messagesToIgnore;
    size_t m_totalMessagesReceived;

    void onMessage(
        std::shared_ptr< AbstractServerConnection > connection,
        stun::Message message)
    {
        ++m_totalMessagesReceived;

        if (m_messagesToIgnore > 0)
        {
            //ignoring message
            --m_messagesToIgnore;
            return;
        }

        nx::stun::Message response(
            stun::Header(
                nx::stun::MessageClass::successResponse,
                nx::stun::bindingMethod,
                message.header.transactionId));
        response.newAttribute<stun::attrs::Nonce>(kServerResponseNonce);
        connection->sendMessage(std::move(response), nullptr);
    }
};

const QByteArray UdpClient::kServerResponseNonce("correctServerResponse");


TEST_F(UdpClient, server_reports_port)
{
    ASSERT_NE(0, anyServerEndpoint().port);
}

/**
 * Common test.
 * Sending request to multiple servers, receiving responses.
 */
TEST_F(UdpClient, client_test_sync)
{
    static const int REQUESTS_TO_SEND = 100;
    static const int LOCAL_SERVERS_COUNT = 10;

    for (int i = 1; i < LOCAL_SERVERS_COUNT; ++i)
        addServer();

    stun::UdpClient client;
    for (int i = 0; i < REQUESTS_TO_SEND; ++i)
    {
        nx::stun::Message requestMessage(
            stun::Header(
                nx::stun::MessageClass::request,
                nx::stun::bindingMethod));

        SystemError::ErrorCode errorCode = SystemError::noError;
        nx::stun::Message response;
        std::tie(errorCode, response) = makeSyncCall<SystemError::ErrorCode, Message>(
            std::bind(
                &stun::UdpClient::sendRequestTo,
                &client,
                anyServerEndpoint(),
                requestMessage,
                std::placeholders::_1));
        ASSERT_EQ(SystemError::noError, errorCode);
        ASSERT_EQ(requestMessage.header.transactionId, response.header.transactionId);
    }

    client.pleaseStopSync();
}

TEST_F(UdpClient, client_test_async)
{
    static const int kRequestToSendCount = 1000;
    static const int kLocalServersCount = 10;
    static const std::chrono::seconds kMaxResponsesWaitTime(
        3 * utils::TestOptions::timeoutMultiplier());

    for (int i = 1; i < kLocalServersCount; ++i)
        addServer();

    stun::UdpClient client;
    std::mutex mutex;
    std::condition_variable cond;
    std::set<nx::String> expectedTransactionIDs;

    for (int i = 0; i < kRequestToSendCount; ++i)
    {
        nx::stun::Message requestMessage(
            stun::Header(
                nx::stun::MessageClass::request,
                nx::stun::bindingMethod));

        {
            std::unique_lock<std::mutex> lk(mutex);
            expectedTransactionIDs.insert(requestMessage.header.transactionId);
        }
        client.sendRequestTo(
            anyServerEndpoint(),
            std::move(requestMessage),
            [&mutex, &cond, &expectedTransactionIDs](
                SystemError::ErrorCode errorCode,
                Message response)
        {
            ASSERT_EQ(SystemError::noError, errorCode);
            std::unique_lock<std::mutex> lk(mutex);
            ASSERT_EQ(1, expectedTransactionIDs.erase(response.header.transactionId));
            if (expectedTransactionIDs.empty())
                cond.notify_all();
        });
    }

    {
        std::unique_lock<std::mutex> lk(mutex);
        ASSERT_TRUE(cond.wait_for(
            lk,
            kMaxResponsesWaitTime,
            [&expectedTransactionIDs]()->bool {return expectedTransactionIDs.empty();}));
        ASSERT_TRUE(expectedTransactionIDs.empty());
    }

    client.pleaseStopSync();
}

/**
 * Testing request retransmission by client.
 * - sending request
 * - ignoring request on server
 * - waiting for retransmission
 * - server sends response
 * - checking that client received response
 */
TEST_F(UdpClient, client_retransmits_general)
{
    ignoreNextMessage();

    stun::UdpClient client;
    nx::stun::Message requestMessage(
        stun::Header(
            nx::stun::MessageClass::request,
            nx::stun::bindingMethod));

    SystemError::ErrorCode errorCode = SystemError::noError;
    nx::stun::Message response;
    std::tie(errorCode, response) = makeSyncCall<SystemError::ErrorCode, Message>(
        std::bind(
            &stun::UdpClient::sendRequestTo,
            &client,
            anyServerEndpoint(),
            requestMessage,
            std::placeholders::_1));
    ASSERT_EQ(SystemError::noError, errorCode);
    ASSERT_EQ(requestMessage.header.transactionId, response.header.transactionId);
    ASSERT_EQ(2, totalMessagesReceived());

    client.pleaseStopSync();
}

/**
 * Checking that client reports failure after sending maximum retransmissions allowed.
 * - sending request
 * - ignoring all request on server
 * - counting retransmission issued by a client
 * - waiting for client to report failure
 */
TEST_F(UdpClient, client_retransmits_max_retransmits)
{
    const int MAX_RETRANSMISSIONS = 5;

    ignoreNextMessage(MAX_RETRANSMISSIONS+1);

    stun::UdpClient client;
    client.setRetransmissionTimeOut(std::chrono::milliseconds(100));
    client.setMaxRetransmissions(MAX_RETRANSMISSIONS);
    nx::stun::Message requestMessage(
        stun::Header(
            nx::stun::MessageClass::request,
            nx::stun::bindingMethod));

    SystemError::ErrorCode errorCode = SystemError::noError;
    nx::stun::Message response;
    std::tie(errorCode, response) = makeSyncCall<SystemError::ErrorCode, Message>(
        std::bind(
            &stun::UdpClient::sendRequestTo,
            &client,
            anyServerEndpoint(),
            requestMessage,
            std::placeholders::_1));
    ASSERT_EQ(SystemError::timedOut, errorCode);
    ASSERT_EQ(MAX_RETRANSMISSIONS+1, totalMessagesReceived());

    client.pleaseStopSync();
}

/**
 * Checking that client reports error to waiters if removed before receiving response.
 */
TEST_F(UdpClient, client_cancellation)
{
    const int REQUESTS_TO_SEND = 3;

    stun::UdpClient client;
    client.setRetransmissionTimeOut(std::chrono::seconds(100));
    nx::stun::Message requestMessage(
        stun::Header(
            nx::stun::MessageClass::request,
            nx::stun::bindingMethod));

    ignoreNextMessage(REQUESTS_TO_SEND);

    int errorsReported = 0;
    auto completionHandler = 
        [&errorsReported](
            SystemError::ErrorCode errorCode,
            nx::stun::Message /*response*/)
        {
            NX_ASSERT(errorCode == SystemError::interrupted);
            ++errorsReported;
        };

    for (int i = 0; i < REQUESTS_TO_SEND; ++i)
    {
        requestMessage.header.transactionId = Header::makeTransactionId();
        client.sendRequestTo(
            anyServerEndpoint(),
            requestMessage,
            completionHandler);
    }

    client.pleaseStopSync();
}

/**
 * Testing that client accepts response from server endpoint only.
 */
TEST_F(UdpClient, client_response_injection)
{
    addServer();

    const auto serverEndpoint = anyServerEndpoint();

    nx::stun::Message requestMessage(
        stun::Header(
            nx::stun::MessageClass::request,
            nx::stun::bindingMethod));

    //ignoring messages so that UDP client uses retransmits
    ignoreNextMessage(3);

    QnMutex mtx;
    QnWaitCondition cond;
    boost::optional<Message> response;
    SystemError::ErrorCode responseErrorCode = SystemError::noError;

    stun::UdpClient client;
    ASSERT_TRUE(client.bind(SocketAddress(HostAddress::localhost, 0)));
    client.sendRequestTo(
        SocketAddress(HostAddress::localhost, serverEndpoint.port),
        //serverEndpoint,
        requestMessage,
        [&mtx, &cond, &response, &responseErrorCode](
            SystemError::ErrorCode errorCode,
            Message msg)
        {
            QnMutexLocker lk(&mtx);
            responseErrorCode = errorCode;
            response = std::move(msg);
            cond.wakeAll();
        });

    //TODO #ak waiting for message to be sent
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    //sending unexpected responses and junk to the client
    auto udpSocket = SocketFactory::createDatagramSocket();
    ASSERT_TRUE(
        udpSocket->setDestAddr(
            SocketAddress(
                HostAddress::localhost,
                client.localAddress().port)));

    const int kPacketsToInject = 20;
    for (int i = 0; i < kPacketsToInject; ++i)
    {
        if ((i == 0) || (nx::utils::random::number(0, 4) == 0))
        {
            nx::stun::Message injectedResponseMessage(
                stun::Header(
                    nx::stun::MessageClass::successResponse,
                    nx::stun::bindingMethod,
                    requestMessage.header.transactionId));
            injectedResponseMessage.newAttribute<stun::attrs::Nonce>("bad");
            const auto serializedMessage = MessageSerializer::serialized(injectedResponseMessage);
            ASSERT_EQ(serializedMessage.size(), udpSocket->send(serializedMessage)) 
                << SystemError::getLastOSErrorText().toStdString();
        }
        else
        {
            const auto buf = nx::utils::generateRandomName(nx::utils::random::number(100, 400));
            ASSERT_EQ(buf.size(), udpSocket->send(buf))
                << SystemError::getLastOSErrorText().toStdString();
        }
    }

    {
        QnMutexLocker lk(&mtx);
        while (!response)
            cond.wait(lk.mutex());
    }

    ASSERT_EQ(SystemError::noError, responseErrorCode);
    const auto* nonceAttr = response->getAttribute<stun::attrs::Nonce>();
    ASSERT_NE(nullptr, nonceAttr);
    ASSERT_EQ(kServerResponseNonce, nonceAttr->getString());

    client.pleaseStopSync();
}

class UdpClientRedirect:
    public UdpClient
{
protected:
    ServerContext& contentServer()
    {
        return server(0);
    }

    ServerContext& redirectionServer()
    {
        return server(1);
    }

    void givenTwoServersWithRedirection()
    {
        using namespace std::placeholders;

        addServer();
        addServer();

        contentServer().messageDispatcher.registerRequestProcessor(
            stun::bindingMethod,
            std::bind(&UdpClientRedirect::processBindingRequest, this, _1, _2));

        redirectionServer().messageDispatcher.registerRequestProcessor(
            stun::bindingMethod,
            std::bind(&UdpClientRedirect::redirectHandler, this, 
                _1, _2, contentServer().server->address()));
    }

    void givenTwoServersWithRedirectionLoop()
    {
    }

    void whenRequestingRedirectedResource()
    {
        stun::Message request(
            stun::Header(
                stun::MessageClass::request,
                stun::bindingMethod,
                stun::Header::makeTransactionId()));

        nx::utils::promise<std::pair<SystemError::ErrorCode, stun::Message>> responsePromise;
        m_client.sendRequestTo(
            redirectionServer().server->address(),
            std::move(request),
            [&responsePromise](SystemError::ErrorCode errorCode, stun::Message response)
            {
                responsePromise.set_value(std::make_pair(errorCode, std::move(response)));
            });
        m_response = responsePromise.get_future().get();
    }

    void thenClientShouldFetchResourceFromActualLocation()
    {
        ASSERT_EQ(SystemError::noError, m_response.first);
        ASSERT_EQ(stun::MessageClass::successResponse, m_response.second.header.messageClass);
        const auto mappedAddress = m_response.second.getAttribute<attrs::MappedAddress>();
        ASSERT_NE(nullptr, mappedAddress);
        ASSERT_EQ(contentServer().server->address(), mappedAddress->endpoint());
    }

    void thenClientShouldPerformFiniteNumberOfRedirectionAttempts()
    {
    }

private:
    void processBindingRequest(
        std::shared_ptr<stun::AbstractServerConnection> connection,
        stun::Message message)
    {
        stun::Message response(stun::Header(
            stun::MessageClass::successResponse,
            stun::bindingMethod,
            message.header.transactionId));
        response.newAttribute<stun::attrs::MappedAddress>(connection->getSourceAddress());
        connection->sendMessage(std::move(response), nullptr);
    }

    void redirectHandler(
        std::shared_ptr<stun::AbstractServerConnection> connection,
        stun::Message message,
        SocketAddress targetAddress)
    {
        stun::Message response(stun::Header(
            stun::MessageClass::errorResponse,
            stun::bindingMethod,
            message.header.transactionId));
        response.newAttribute<stun::attrs::ErrorDescription>(stun::error::tryAlternate);
        response.newAttribute<stun::attrs::AlternateServer>(connection->getSourceAddress());
        connection->sendMessage(std::move(response), nullptr);
    }

private:
    stun::UdpClient m_client;
    std::pair<SystemError::ErrorCode, stun::Message> m_response;
};

TEST_F(UdpClientRedirect, simple_redirect_by_300_and_alternate_server)
{
    givenTwoServersWithRedirection();
    whenRequestingRedirectedResource();
    thenClientShouldFetchResourceFromActualLocation();
}

TEST_F(UdpClientRedirect, no_infinite_redirect_loop)
{
    //givenTwoServersWithRedirectionLoop();
    //whenRequestingRedirectedResource();
    //thenClientShouldPerformFiniteNumberOfRedirectionAttempts();
}

TEST_F(UdpClientRedirect, redirect_of_authorized_resource)
{
}

} // namespace test
} // namespace stun
} // namespace nx
