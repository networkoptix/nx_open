// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "tunneling_acceptance_tests.h"

#include <nx/network/http/rest/http_rest_client.h>
#include <nx/network/http/tunneling/detail/request_paths.h>
#include <nx/network/system_socket.h>
#include <nx/utils/scope_guard.h>

namespace nx::network::http::tunneling::test {

struct ExperimentalMethodMask { static constexpr int value = TunnelMethod::experimental; };

class SeparateUpDownConnectionsTunnel:
    public HttpTunneling<ExperimentalMethodMask>
{
    using base_type = HttpTunneling<ExperimentalMethodMask>;

public:
    ~SeparateUpDownConnectionsTunnel()
    {
        for (;;)
        {
            auto httpClient = m_completedOpenDownChannelRequests.pop(
                std::chrono::milliseconds::zero());
            if (!httpClient || !*httpClient)
                break;
            (*httpClient)->pleaseStopSync();
        }

        std::for_each(
            m_httpPipes.begin(), m_httpPipes.end(),
            [](auto& pipe) { pipe->pleaseStopSync(); });
    }

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        givenTunnellingServer();
    }

    void whenOpenDownChannel()
    {
        auto httpClient = std::make_unique<AsyncClient>(ssl::kAcceptAnyCertificate);
        auto httpClientPtr = httpClient.get();

        httpClientPtr->setOnResponseReceived(
            [this, httpClient = std::move(httpClient)]() mutable
            {
                saveDownChannelConnection(std::move(httpClient));
            });

        httpClientPtr->doGet(
            downChannelUrl(),
            [this]() { saveDownChannelConnection(nullptr); });
    }

    void whenSendOpenTunnelRequestsTogether(const std::array<Method, 2> methods)
    {
        initializeHttpPipe();

        m_httpPipes.back()->executeInAioThreadSync(
            [this, methods]()
            {
                nx::Buffer requestBuf;
                addTunnelRequest(
                    methods[0],
                    methods[0] == Method::post ? upChannelUrl() : downChannelUrl(),
                    &requestBuf);

                addTunnelRequest(
                    methods[1],
                    methods[1] == Method::post ? upChannelUrl() : downChannelUrl(),
                    &requestBuf);

                m_httpPipes.back()->sendData(requestBuf, [](auto /*result*/) {});
            });
    }

    void thenDownChannelIsOpened()
    {
        auto httpClient = m_completedOpenDownChannelRequests.pop();
        ASSERT_NE(nullptr, httpClient);

        auto httpClientGuard = nx::utils::makeScopeGuard(
            [&httpClient]() { httpClient->pleaseStopSync(); });

        ASSERT_FALSE(httpClient->failed());
        ASSERT_EQ(StatusCode::ok, httpClient->response()->statusLine.statusCode);
    }

    void waitUntilServerClosesConnectionGracefully()
    {
        ASSERT_TRUE(m_downChannelConnection->setNonBlockingMode(false)
            && m_downChannelConnection->setRecvTimeout(kNoTimeout))
            << SystemError::getLastOSErrorText();

        for (;;)
        {
            char buf[128];
            auto bytesRead = m_downChannelConnection->recv(buf, sizeof(buf));
            ASSERT_GE(bytesRead, 0) << SystemError::getLastOSErrorText();
            if (bytesRead == 0)
                break;
        }
    }

    void thenResponseIsReceived(StatusCode::Value expected)
    {
        auto msg = m_responses.pop();
        ASSERT_EQ(MessageType::response, msg.type);
        ASSERT_EQ(expected, msg.response->statusLine.statusCode);
    }

    void thenConnectionIsClosed()
    {
        m_connectionClosedEvents.pop();
    }

private:
    nx::utils::SyncQueue<std::unique_ptr<AsyncClient>> m_completedOpenDownChannelRequests;
    std::unique_ptr<AbstractStreamSocket> m_downChannelConnection;
    nx::Buffer m_requestBuf;
    std::vector<std::unique_ptr<AsyncMessagePipeline>> m_httpPipes;
    nx::utils::SyncQueue<Message> m_responses;
    nx::utils::SyncQueue<
        std::tuple<AsyncMessagePipeline*, SystemError::ErrorCode, bool>> m_connectionClosedEvents;

    nx::utils::Url downChannelUrl()
    {
        return channelUrl(detail::kExperimentalTunnelDownPath);
    }

    nx::utils::Url upChannelUrl()
    {
        return channelUrl(detail::kExperimentalTunnelUpPath);
    }

    nx::utils::Url channelUrl(const std::string& path)
    {
        return nx::network::url::Builder(baseUrl()).appendPath(
            rest::substituteParameters(path, {"testTunnelId"}))
            .toUrl();
    }

    void initializeHttpPipe()
    {
        auto connection = std::make_unique<TCPSocket>(AF_INET);
        ASSERT_TRUE(connection->connect(url::getEndpoint(downChannelUrl()), kNoTimeout)
            && connection->setNonBlockingMode(true))
            << SystemError::getLastOSErrorText();
        auto httpPipe = std::make_unique<AsyncMessagePipeline>(std::move(connection));

        m_httpPipes.push_back(std::move(httpPipe));
        m_httpPipes.back()->setMessageHandler(
            [this](http::Message msg)
            {
                m_responses.push(std::move(msg));
            });
        m_httpPipes.back()->registerCloseHandler(
            [this, ptr = m_httpPipes.back().get()](
                SystemError::ErrorCode reason, bool connectionDestroyed)
            {
                m_connectionClosedEvents.push(std::make_tuple(ptr, reason, connectionDestroyed));
            });
        m_httpPipes.back()->startReadingConnection();
    }

    void saveDownChannelConnection(std::unique_ptr<AsyncClient> httpClient)
    {
        if (httpClient)
            m_downChannelConnection = httpClient->takeSocket();
        m_completedOpenDownChannelRequests.push(std::move(httpClient));
    }

    void addTunnelRequest(Method method, const nx::utils::Url& url, nx::Buffer* buf)
    {
        Message msg(MessageType::request);
        msg.request->requestLine.method = method;
        msg.request->requestLine.url.setPath(url.path());
        msg.request->requestLine.version = http_1_1;
        if (method == Method::post)
            msg.request->headers.emplace("Content-Length", "0");
        msg.serialize(buf);
    }
};

TEST_F(SeparateUpDownConnectionsTunnel, incomplete_tunnel_is_closed_by_timeout)
{
    setHttpConnectionInactivityTimeout(std::chrono::milliseconds(10));

    whenOpenDownChannel();
    thenDownChannelIsOpened();

    waitUntilServerClosesConnectionGracefully();
}

TEST_F(SeparateUpDownConnectionsTunnel, tunnel_through_a_single_connection_is_denied)
{
    whenSendOpenTunnelRequestsTogether({Method::get, Method::post});

    thenResponseIsReceived(StatusCode::ok); //< Response to GET.
    thenConnectionIsClosed(); //< No response to POST since GET has an infinite body.
}

TEST_F(SeparateUpDownConnectionsTunnel, tunnel_through_a_single_connection_is_denied_2)
{
    whenSendOpenTunnelRequestsTogether({Method::post, Method::get});

    thenResponseIsReceived(StatusCode::ok); //< Response to POST.
    thenConnectionIsClosed(); //< No response to GET since POST was supposed to have an infinite body.
}

} // namespace nx::network::http::tunneling::test
