#pragma once

#include <nx/network/stun/async_client.h>
#include <nx/network/stun/stun_types.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/thread/sync_queue.h>

#include "mediator_functional_test.h"

namespace nx::hpm::test {

template<typename StunClientType>
// requires std::is_base_of<nx::network::stun::AbstractAsyncClient, StunClientType>::value
class ApiTestFixture:
    public MediatorFunctionalTest
{
public:
    ~ApiTestFixture()
    {
        if (m_connection)
            m_connection->pleaseStopSync();

        if (m_client)
            m_client->pleaseStopSync();
    }

protected:
    virtual void SetUp() override
    {
        ASSERT_TRUE(startAndWaitUntilStarted());
    }

    virtual nx::utils::Url stunApiUrl() const = 0;

    void openInactiveConnection()
    {
        m_connection = std::make_unique<nx::network::TCPSocket>(AF_INET);
        ASSERT_TRUE(m_connection->connect(stunTcpEndpoint(), nx::network::kNoTimeout));
    }

    void waitForConnectionIsClosedByMediator()
    {
        if (m_connection)
            waitForRawConnectionIsClosedByServer();
        else if (m_client)
            waitForClientConnectionIsClosedByServer();
    }

    void whenOpenConnectionToMediator()
    {
        if (!m_client)
            initializeClient();
    }

    void whenSendMalformedRequest()
    {
        if (!m_client)
            initializeClient();

        nx::network::stun::Message request(nx::network::stun::Header(
            nx::network::stun::MessageClass::request,
            nx::network::stun::extension::methods::bind));
        m_client->sendRequest(
            request,
            [this](auto&&... args) { saveResponse(std::forward<decltype(args)>(args)...); });
    }

    void thenConnectSucceeded()
    {
        ASSERT_EQ(SystemError::noError, m_connectionDoneEvents.pop());
    }

    void thenReponseIsReceived()
    {
        m_prevRequestResult = m_requestResults.pop();
        ASSERT_EQ(SystemError::noError, std::get<0>(m_prevRequestResult));
    }

    void andResponseCodeIs(api::ResultCode expected)
    {
        ASSERT_EQ(expected, std::get<1>(m_prevRequestResult));
    }

    void assertConnectionIsNotClosedDuring(std::chrono::milliseconds delay)
    {
        ASSERT_TRUE(m_connection->setRecvTimeout(delay.count()));

        std::array<char, 256> readBuf;
        ASSERT_EQ(-1, m_connection->recv(readBuf.data(), readBuf.size(), 0));
        ASSERT_TRUE(
            SystemError::getLastOSErrorCode() == SystemError::timedOut ||
            SystemError::getLastOSErrorCode() == SystemError::wouldBlock);
    }

private:
    using RequestResult = std::tuple<SystemError::ErrorCode, api::ResultCode>;

    std::unique_ptr<nx::network::TCPSocket> m_connection;
    std::unique_ptr<nx::network::stun::AbstractAsyncClient> m_client;
    nx::utils::SyncQueue<RequestResult> m_requestResults;
    nx::utils::SyncQueue<SystemError::ErrorCode> m_connectionClosureEvents;
    nx::utils::SyncQueue<SystemError::ErrorCode> m_connectionDoneEvents;
    RequestResult m_prevRequestResult;

    void initializeClient()
    {
        m_client = std::make_unique<StunClientType>();
        m_client->setOnConnectionClosedHandler(
            [this](auto&&... args) { saveConnectionClosureEvent(std::forward<decltype(args)>(args)...); });
        m_client->connect(
            stunApiUrl(),
            [this](auto&&... args) { saveConnectionDoneEvent(std::forward<decltype(args)>(args)...); });
    }

    void saveResponse(
        SystemError::ErrorCode resultCode,
        nx::network::stun::Message response)
    {
        const auto resultCodeAttr =
            response.getAttribute<network::stun::extension::attrs::ResultCode>();

        m_requestResults.push(std::make_tuple(
            resultCode,
            resultCodeAttr ? resultCodeAttr->value() : api::ResultCode::responseParseError));
    }

    void saveConnectionClosureEvent(SystemError::ErrorCode reasonCode)
    {
        m_connectionClosureEvents.push(reasonCode);
    }

    void saveConnectionDoneEvent(SystemError::ErrorCode resultCode)
    {
        m_connectionDoneEvents.push(resultCode);
    }

    void waitForRawConnectionIsClosedByServer()
    {
        std::array<char, 256> readBuf;
        for (;;)
        {
            const int bytesRead = m_connection->recv(readBuf.data(), readBuf.size(), 0);
            if (bytesRead > 0)
                continue;
            if (bytesRead == 0 ||
                nx::network::socketCannotRecoverFromError(SystemError::getLastOSErrorCode()))
            {
                break;
            }
        }
    }

    void waitForClientConnectionIsClosedByServer()
    {
        m_connectionClosureEvents.pop();
    }
};

}
