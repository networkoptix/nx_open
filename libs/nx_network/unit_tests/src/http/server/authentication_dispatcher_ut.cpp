#include <gtest/gtest.h>

#include <nx/network/http/server/authentication_dispatcher.h>

#include <nx/network/http/http_client.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/url/url_parse_helper.h>

namespace nx::network::http::server::test {

namespace {

static const std::vector<const char *> kTestPaths =
{
    "/test/get",
    "/test/delete",
    "/test/unregisteredput"
};

} // namespace

class AuthenticationDispatcher:
    public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(m_httpServer.bindAndListen());

        m_httpClient.setResponseReadTimeout(kNoTimeout);
        m_httpClient.setMessageBodyReadTimeout(kNoTimeout);

        registerRequestHandlers(&m_httpServer.httpMessageDispatcher());
        initializeAndRegisterAuthenticators();
    }

    void registerRequestHandlers(
        http::server::rest::MessageDispatcher* messageDispatcher)
    {
        messageDispatcher->registerRequestProcessorFunc(
            http::Method::get,
            kTestPaths[0], //< "/test/get"
            [this](auto&&... args) { serveTestGet(std::move(args)...); });

        messageDispatcher->registerRequestProcessorFunc(
            http::Method::delete_,
            kTestPaths[1], //< "/test/delete"
            [this](auto&&... args) { serveTestDelete(std::move(args)...); });

        messageDispatcher->registerRequestProcessorFunc(
            http::Method::put,
            kTestPaths[2], //< "/test/unregistered"
            [this](auto&&... args) { serveTestUnregisteredPut(std::move(args)...); });
    }

    void serveTestGet(
        http::RequestContext requestContext,
        http::RequestProcessedHandler completionHandler)
    {
        m_dispatcher.authenticate(
            *requestContext.connection,
            requestContext.request,
            [&](server::AuthenticationResult result)
            {
                ASSERT_TRUE(result.isSucceeded);
            });

        completionHandler(http::StatusCode::ok);
    }

    void serveTestDelete(
        http::RequestContext requestContext,
        http::RequestProcessedHandler completionHandler)
    {
        // "DELETE" method has authentication set to always fail,
        // so we expect this path authentication to fail.
        m_dispatcher.authenticate(
            *requestContext.connection,
            requestContext.request,
            [&](server::AuthenticationResult result)
            {
                ASSERT_FALSE(result.isSucceeded);
            });

        completionHandler(http::StatusCode::ok);
    }

    void serveTestUnregisteredPut(
        http::RequestContext requestContext,
        http::RequestProcessedHandler completionHandler)
    {
        // Unregistered "PUT" method does not have an authenticator,
        // so we expect the authentication dispatcher to authenticate by default.
        m_dispatcher.authenticate(
            *requestContext.connection,
            requestContext.request,
            [&](server::AuthenticationResult result)
            {
                ASSERT_TRUE(result.isSucceeded);
            });

        completionHandler(http::StatusCode::ok);
    }

    nx::utils::Url createUrl(const std::string& fullPath)
    {
        return url::Builder()
            .setScheme(http::kUrlSchemeName)
            .setEndpoint(m_httpServer.serverAddress())
            .setPath(fullPath);
    }

    void whenAuthenticateAllRegisteredAuthenticators()
    {
        // "/test/get"
        ASSERT_TRUE(m_httpClient.doGet(createUrl(kTestPaths[0])));

        // "/test/delete"
        ASSERT_TRUE(m_httpClient.doDelete(createUrl(kTestPaths[1])));
    }

    void whenAuthenticateUnregisteredAuthenticator()
    {
        // "/test/unregisteredput"
        ASSERT_TRUE(m_httpClient.doPut(createUrl(kTestPaths[2]), "/application/json", {}));
    }

private:
        void initializeAndRegisterAuthenticators()
        {
            // Skipping kTestPaths[2]: "/test/unregisteredput"
            for (std::size_t i = 0; i < kTestPaths.size() - 1; ++i)
            {
                bool authenticationResult = std::string(kTestPaths[i]) == "/test/delete"
                    ? false
                    : true;

                auto authenticationManager = std::make_unique<TestAuthenticationManager>(nullptr);

                // Force dummy authentication
                authenticationManager->setAuthenticationEnabled(false);
                authenticationManager->setAuthenticationSucceeded(authenticationResult);

                m_dispatcher.add(std::regex(kTestPaths[i]), authenticationManager.get());

                m_authenticators.emplace(std::move(authenticationManager));
            }
        }

protected:
    std::set<std::unique_ptr<TestAuthenticationManager>> m_authenticators;

    server::AuthenticationDispatcher m_dispatcher;
    http::TestHttpServer m_httpServer;
    http::HttpClient m_httpClient;
};

TEST_F(AuthenticationDispatcher, dispatches_matched_requests_to_registered_authenticator)
{
    whenAuthenticateAllRegisteredAuthenticators();

    //thenRequestSucceeded();
}

TEST_F(AuthenticationDispatcher, authenticates_when_no_authenticator_is_matched)
{
    whenAuthenticateUnregisteredAuthenticator();

    //thenRequestSucceeded();
}


} // namespace nx::network::http::server::test