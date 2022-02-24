// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <memory>
#include <optional>
#include <system_error>

#include <nx/utils/url.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/http/futures/client.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/socket_factory.h>
#include <nx/network/system_socket.h>

#include <QString>
#include <gtest/gtest.h>

namespace nx::network::http::futures::test {

using namespace nx::utils;

class FailingStreamSocket:
    public TCPSocket
{
public:
    virtual void connectAsync(
        const SocketAddress& /*addr*/,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) override
    {
        post([handler = std::move(handler)]() { handler(SystemError::connectionRefused); });
    }
};

//-------------------------------------------------------------------------------------------------

class FuturesHttpClient: public ::testing::Test
{
public:
    static constexpr char kFooPath[] = "/foo";
    static constexpr char kExpectedBody[] = "Hello, world!";

    std::optional<TestHttpServer> server{std::in_place};
    std::optional<Client> client{std::in_place, ssl::kAcceptAnyCertificate};

    ~FuturesHttpClient()
    {
        if (m_socketFactoryBak)
        {
            SocketFactory::setCreateStreamSocketFunc(std::move(*m_socketFactoryBak));
            m_socketFactoryBak = std::nullopt;
        }
    }

    Url expandUrl(const std::string& path)
    {
        return url::Builder()
            .setScheme("http")
            .setEndpoint(server->serverAddress())
            .setPath(path)
            .toUrl();
    }

    void whenGettable(const std::string& path, nx::Buffer body)
    {
        ASSERT_TRUE(server->registerStaticProcessor(
            path, std::move(body), "text/plain", Method::get));
        ASSERT_TRUE(server->bindAndListen());
    }

    void installFailingSocket()
    {
        m_socketFactoryBak = SocketFactory::setCreateStreamSocketFunc(
            [](auto&&...) { return std::make_unique<FailingStreamSocket>(); });
    }

private:
    std::optional<SocketFactory::CreateStreamSocketFuncType> m_socketFactoryBak;
};

TEST_F(FuturesHttpClient, get_ok)
{
    whenGettable(kFooPath, kExpectedBody);

    auto future = client->get(expandUrl(kFooPath));
    const auto response = future.get();
    ASSERT_EQ(response.messageBody, kExpectedBody);
}

TEST_F(FuturesHttpClient, get_throws_system_error)
{
    installFailingSocket();

    whenGettable(kFooPath, kExpectedBody);

    auto future = client->get("http://127.0.0.1:0/");
    try
    {
        future.get();

        FAIL();
    }
    catch (const std::system_error& exception)
    {
        ASSERT_EQ(exception.code(), std::errc::connection_refused);
    }
}

TEST_F(FuturesHttpClient, get_canceled)
{
    whenGettable(kFooPath, kExpectedBody);

    cf::future<Response> future;
    client->executeInAioThreadSync([&]{
        future = client->get(expandUrl(kFooPath));
        client = std::nullopt;
    });
    try
    {
        future.get();

        FAIL();
    }
    catch (const std::system_error& exception)
    {
        ASSERT_EQ(exception.code(), std::errc::operation_canceled);
    }
}

TEST_F(FuturesHttpClient, post_ok)
{
    ASSERT_TRUE(server->registerRequestProcessorFunc(kFooPath,
        [](auto&& context, auto&& handler)
        {
            handler((context.request.messageBody == kExpectedBody)
                ? StatusCode::created
                : StatusCode::badRequest);
        }, Method::post));
    ASSERT_TRUE(server->bindAndListen());

    auto future = client->post(expandUrl(kFooPath), "text/plain", kExpectedBody);
    const auto response = future.get();
    ASSERT_EQ(response.statusLine.statusCode, StatusCode::created);
}

} // namespace nx::network::http::futures::test
