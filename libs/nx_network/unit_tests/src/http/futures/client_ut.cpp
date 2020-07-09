#include <memory>
#include <optional>
#include <system_error>

#include <nx/utils/url.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/http/futures/client.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/http/buffer_source.h>

#include <QString>
#include <gtest/gtest.h>

namespace nx::network::http::futures::test {

using namespace nx::utils;

class FuturesHttpClient: public ::testing::Test
{
public:
    static inline const QString kFooPath = "/foo";
    static inline const BufferType kExpectedBody = "Hello, world!";

    std::optional<TestHttpServer> server{std::in_place};
    std::optional<Client> client{std::in_place};

    ~FuturesHttpClient()
    {
        if (client)
            client->executeInAioThreadSync([&]{ client = std::nullopt; });
        if (server)
            server->server().executeInAioThreadSync([&]{ server = std::nullopt; });
    }

    Url expandUrl(const QString& path)
    {
        return url::Builder()
            .setScheme("http")
            .setEndpoint(server->serverAddress())
            .setPath(path)
            .toUrl();
    }

    void whenGettable(const QString& path, BufferType body)
    {
        ASSERT_TRUE(server->registerStaticProcessor(path,
            std::move(body), "text/plain", Method::get));
        ASSERT_TRUE(server->bindAndListen());
    }
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

    auto future = client->get(expandUrl(kFooPath));
    client->executeInAioThreadSync([&]{ client = std::nullopt; });
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
