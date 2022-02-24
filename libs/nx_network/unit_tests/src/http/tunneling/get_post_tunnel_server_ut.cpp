// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "tunneling_acceptance_tests.h"

#include <memory>

#include <nx/network/http/http_async_client.h>
#include <nx/network/http/rest/http_rest_client.h>
#include <nx/network/http/tunneling/detail/request_paths.h>
#include <nx/network/system_socket.h>

namespace nx::network::http::tunneling::test {

struct GetPostMethodMask { static constexpr int value = TunnelMethod::getPost; };
class HttpTunnelingGetPost:
    public HttpTunneling<GetPostMethodMask>
{
public:
    ~HttpTunnelingGetPost()
    {
        if (m_httpClient)
            m_httpClient->pleaseStopSync();

        if (m_tunnelConnection)
            m_tunnelConnection->pleaseStopSync();
    }

protected:
    void givenTunnellingServerWithAuthentication()
    {
        const auto credentials = Credentials("test", PasswordAuthToken("test"));

        httpServer().enableAuthentication(".*");
        httpServer().registerUserCredentials(credentials);

        givenTunnellingServer();

        auto url = baseUrl();
        url.setUserName(credentials.username);
        url.setPassword(credentials.authToken.value);
        setBaseUrl(url);
    }

    void whenSendGetRequest()
    {
        const auto url = nx::network::url::Builder(baseUrl())
            .appendPath(rest::substituteParameters(detail::kGetPostTunnelPath, {"1"})).toUrl();

        m_httpClient = std::make_unique<AsyncClient>(ssl::kAcceptAnyCertificate);
        m_httpClient->setTimeouts(AsyncClient::kInfiniteTimeouts);
        std::promise<StatusCode::Value> done;
        m_httpClient->doGet(
            url,
            [this, &done]()
            {
                m_tunnelConnection = m_httpClient->takeSocket();
                done.set_value((StatusCode::Value) m_httpClient->response()->statusLine.statusCode);
            });
        ASSERT_EQ(StatusCode::ok, done.get_future().get());
    }

    void whenSendGetAndPostRequestsTogether()
    {
        const auto url = nx::network::url::Builder(baseUrl())
            .appendPath(rest::substituteParameters(detail::kGetPostTunnelPath, {"1"})).toUrl();

        nx::Buffer buf;
        addTunnelGetRequest(url, &buf);
        addTunnelPostRequest(url, &buf);
        sendBuf(url::getEndpoint(url), buf);
    }

    void whenSendGetAndPostAndUserRequestsTogether()
    {
        const auto url = nx::network::url::Builder(baseUrl())
            .appendPath(rest::substituteParameters(detail::kGetPostTunnelPath, {"1"})).toUrl();

        nx::Buffer buf;
        addTunnelGetRequest(url, &buf);
        addTunnelPostRequest(url, &buf);
        addUserRequest(&buf);
        sendBuf(url::getEndpoint(url), buf);
    }

    void thenTunnelIsClosedByServerEventually()
    {
        ASSERT_TRUE(m_tunnelConnection->setNonBlockingMode(false));
        ASSERT_TRUE(m_tunnelConnection->setRecvTimeout(kNoTimeout));

        for (;;)
        {
            char buf[128];
            const auto bytesReceived = m_tunnelConnection->recv(buf, sizeof(buf));
            if (bytesReceived < 0 && SystemError::getLastOSErrorCode() == SystemError::interrupted)
                continue;

            ASSERT_EQ(0, bytesReceived);
            break;
        }
    }

    void andUserRequestIsDelivered()
    {
        ASSERT_TRUE(lastServerTunnelConnection()->setNonBlockingMode(false));

        nx::Buffer buf;
        buf.resize(16 * 1024);
        const auto bytesRead = lastServerTunnelConnection()->recv(buf.data(), buf.size());
        ASSERT_GT(bytesRead, 0) << SystemError::getLastOSErrorText();
        buf.resize(bytesRead);
        ASSERT_EQ(m_userRequest.toString(), buf);
    }

private:
    std::unique_ptr<AsyncClient> m_httpClient;
    std::unique_ptr<AbstractStreamSocket> m_tunnelConnection;
    std::unique_ptr<AbstractStreamSocket> m_rawClientConnection;
    Request m_userRequest;

    void addTunnelGetRequest(const nx::utils::Url& url, nx::Buffer* buf)
    {
        Message getRequest(MessageType::request);
        getRequest.request->requestLine.method = Method::get;
        getRequest.request->requestLine.url.setPath(url.path());
        getRequest.request->requestLine.version = http_1_1;
        getRequest.serialize(buf);
    }

    void addTunnelPostRequest(const nx::utils::Url& url, nx::Buffer* buf)
    {
        Message postRequest(MessageType::request);
        postRequest.request->requestLine.method = Method::post;
        postRequest.request->requestLine.url.setPath(url.path());
        postRequest.request->requestLine.version = http_1_1;
        postRequest.serialize(buf);
    }

    void addUserRequest(nx::Buffer* buf)
    {
        m_userRequest.requestLine.method = Method::get;
        m_userRequest.requestLine.url.setPath("/foo");
        m_userRequest.requestLine.version = http_1_1;
        m_userRequest.headers.emplace("Content-Length", "0");

        Message message(MessageType::request);
        *message.request = m_userRequest;
        message.serialize(buf);
    }

    void sendBuf(const SocketAddress& endpoint, const nx::Buffer& buf)
    {
        m_rawClientConnection = std::make_unique<TCPSocket>(AF_INET);
        ASSERT_TRUE(m_rawClientConnection->connect(endpoint, kNoTimeout));
        ASSERT_EQ(buf.size(), m_rawClientConnection->send(buf.data(), buf.size()));
    }
};

TEST_F(HttpTunnelingGetPost, connection_is_closed_by_timeout_if_post_never_comes)
{
    setHttpConnectionInactivityTimeout(std::chrono::milliseconds(100));

    givenTunnellingServer();
    whenSendGetRequest();
    thenTunnelIsClosedByServerEventually();
}

TEST_F(HttpTunnelingGetPost, get_and_post_arrive_together)
{
    givenTunnellingServer();

    whenSendGetAndPostRequestsTogether();

    thenServerTunnelSucceeded();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // The process does not crash.
}

TEST_F(HttpTunnelingGetPost, data_stuck_to_post_is_not_lost)
{
    givenTunnellingServer();

    whenSendGetAndPostAndUserRequestsTogether();

    thenServerTunnelSucceeded();
    andUserRequestIsDelivered();
}

TEST_F(HttpTunnelingGetPost, authentication_is_supported)
{
    givenTunnellingServerWithAuthentication();

    whenRequestTunnel();

    thenTunnelIsEstablished();
    assertDataExchangeWorksThroughTheTunnel();
}

} // namespace nx::network::http::tunneling::test
