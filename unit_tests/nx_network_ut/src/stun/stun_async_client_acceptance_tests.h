#pragma once

#include <memory>

#include <gtest/gtest.h>

#include <nx/network/url/url_parse_helper.h>
#include <nx/network/stun/abstract_async_client.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx {
namespace stun {
namespace test {

//struct AsyncClientTestTypes
//{
//    using ClientType = ...;
//    using ServerType = ...;
//};

//class StunServer
//{
//public:
//    void pleaseStopSync();
//
//    bool bind(SocketAddress);
//    bool listen();
//
//    QUrl getServerUrl() const;
//};

template<typename AsyncClientTestTypes>
class StunAsyncClientAcceptanceTest:
    public ::testing::Test
{
public:
    ~StunAsyncClientAcceptanceTest()
    {
        m_client.pleaseStopSync();
    }

protected:
    void givenClientWithIndicationHandler()
    {
        ASSERT_TRUE(addIndicationHandler());
    }

    void givenConnectedClient()
    {
        nx::utils::promise<SystemError::ErrorCode> connected;
        m_client.connect(
            m_server->getServerUrl(),
            [&connected](SystemError::ErrorCode resultCode) { connected.set_value(resultCode); });
        ASSERT_EQ(SystemError::noError, connected.get_future().get());
    }

    void whenRemoveHandler()
    {
        nx::utils::promise<void> done;
        m_client.cancelHandlers(this, [&done]() { done.set_value(); });
        done.get_future().wait();
    }

    void whenRestartServer()
    {
        m_server.reset();

        startServer();
    }

    void whenIssueRequest()
    {
        using namespace std::placeholders;

        stun::Message request(stun::Header(
            stun::MessageClass::request,
            m_testMethodNumber));
        m_client.sendRequest(
            std::move(request),
            std::bind(&StunAsyncClientAcceptanceTest::storeRequestResult, this, _1, _2));
    }

    void thenSameHandlerCannotBeAdded()
    {
        ASSERT_FALSE(addIndicationHandler());
    }

    void thenSameHandlerCanBeAddedAgain()
    {
        ASSERT_TRUE(addIndicationHandler());
    }

    void thenClientReconnects()
    {
        m_reconnectEvents.pop();
    }

    void thenSuccessResponseIsReceived()
    {
        m_prevRequestResult = m_requestResult.pop();

        ASSERT_EQ(SystemError::noError, m_prevRequestResult.sysErrorCode);
        ASSERT_EQ(
            stun::MessageClass::successResponse,
            m_prevRequestResult.response.header.messageClass);
    }

    void thenClientIsAbleToPerformRequests()
    {
        whenIssueRequest();
        thenSuccessResponseIsReceived();
    }

private:
    struct RequestResult
    {
        SystemError::ErrorCode sysErrorCode = SystemError::noError;
        stun::Message response;

        RequestResult() = default;
        RequestResult(
            SystemError::ErrorCode sysErrorCode,
            stun::Message response)
            :
            sysErrorCode(sysErrorCode),
            response(std::move(response))
        {
        }
    };

    typename AsyncClientTestTypes::ClientType m_client;
    std::unique_ptr<typename AsyncClientTestTypes::ServerType> m_server;
    SocketAddress m_serverEndpoint = SocketAddress::anyPrivateAddress;
    nx::utils::SyncQueue<int /*dummy*/> m_reconnectEvents;
    nx::utils::SyncQueue<RequestResult> m_requestResult;
    RequestResult m_prevRequestResult;
    const int m_testMethodNumber = stun::MethodType::userMethod + 1;

    virtual void SetUp() override
    {
        startServer();

        m_client.addOnReconnectedHandler(
            std::bind(&StunAsyncClientAcceptanceTest::onReconnected, this));
    }

    void startServer()
    {
        using namespace std::placeholders;

        m_server = std::make_unique<typename AsyncClientTestTypes::ServerType>();

        m_server->dispatcher().registerRequestProcessor(
            m_testMethodNumber,
            std::bind(&StunAsyncClientAcceptanceTest::sendResponse, this, _1, _2));

        ASSERT_TRUE(m_server->bind(m_serverEndpoint));
        m_serverEndpoint = nx::network::url::getEndpoint(m_server->getServerUrl());
        ASSERT_TRUE(m_server->listen());
    }

    void sendResponse(
        std::shared_ptr<stun::AbstractServerConnection> connection,
        nx::stun::Message request)
    {
        nx::stun::Message response(
            stun::Header(
                stun::MessageClass::successResponse,
                request.header.method,
                request.header.transactionId));
        connection->sendMessage(std::move(response));
    }

    void storeRequestResult(
        SystemError::ErrorCode sysErrorCode,
        stun::Message response)
    {
        m_requestResult.push(RequestResult(sysErrorCode, std::move(response)));
    }

    bool addIndicationHandler()
    {
        return m_client.setIndicationHandler(
            nx::stun::MethodType::userIndication + 1,
            [](nx::stun::Message) {},
            this);
    }

    void onReconnected()
    {
        m_reconnectEvents.push(0);
    }
};

TYPED_TEST_CASE_P(StunAsyncClientAcceptanceTest);

TYPED_TEST_P(StunAsyncClientAcceptanceTest, same_handler_cannot_be_added_twice)
{
    this->givenClientWithIndicationHandler();
    this->thenSameHandlerCannotBeAdded();
}

TYPED_TEST_P(StunAsyncClientAcceptanceTest, add_remove_indication_handler)
{
    this->givenClientWithIndicationHandler();
    this->whenRemoveHandler();
    this->thenSameHandlerCanBeAddedAgain();
}

TYPED_TEST_P(StunAsyncClientAcceptanceTest, reconnect_works)
{
    this->givenConnectedClient();
    this->whenRestartServer();
    this->thenClientReconnects();
    this->thenClientIsAbleToPerformRequests();
}

REGISTER_TYPED_TEST_CASE_P(StunAsyncClientAcceptanceTest, 
    same_handler_cannot_be_added_twice, add_remove_indication_handler, reconnect_works);

} // namespace test
} // namespace stun
} // namespace nx
