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

class TestAuthenticator: public AbstractAuthenticationManager
{
public:
    TestAuthenticator():
        m_authenticationManager(std::make_unique<TestAuthenticationManager>(nullptr))
    {
        m_authenticationManager->setAuthenticationEnabled(false);
    }

    virtual ~TestAuthenticator() override = default;

    void authenticate(
        const nx::network::http::HttpServerConnection& connection,
        const nx::network::http::Request& request,
        nx::network::http::server::AuthenticationCompletionHandler completionHandler) override
    {
        m_authenticateCalled = true;
        m_authenticationManager->authenticate(connection, request, std::move(completionHandler));
    }

    bool authenticateCalled() const
    {
        return m_authenticateCalled;
    }

private:
    bool m_authenticateCalled = false;
    std::unique_ptr<TestAuthenticationManager> m_authenticationManager;
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
        std::string requestPath = requestContext.request.requestLine.url.path().toStdString();

        m_dispatcher.authenticate(
            *requestContext.connection,
            requestContext.request,
            [this, requestPath](server::AuthenticationResult result)
            {
                TestAuthenticator* authenticator = m_authenticators[requestPath].get();
                ASSERT_TRUE(authenticator);
                ASSERT_TRUE(authenticator->authenticateCalled());
            });

        completionHandler(http::StatusCode::ok);
    }

    void serveTestDelete(
        http::RequestContext requestContext,
        http::RequestProcessedHandler completionHandler)
    {
        std::string requestPath = requestContext.request.requestLine.url.path().toStdString();

        // "DELETE" method has authentication set to always fail,
        // so we expect this path authentication to fail.
        m_dispatcher.authenticate(
            *requestContext.connection,
            requestContext.request,
            [this, requestPath](server::AuthenticationResult result)
            {
                TestAuthenticator* authenticator = m_authenticators[requestPath].get();
                ASSERT_TRUE(authenticator);
                ASSERT_TRUE(authenticator->authenticateCalled());
            });

        completionHandler(http::StatusCode::ok);
    }

    void serveTestUnregisteredPut(
        http::RequestContext requestContext,
        http::RequestProcessedHandler completionHandler)
    {
        std::string requestPath = requestContext.request.requestLine.url.path().toStdString();

        // Unregistered "PUT" method does not have an authenticator,
        // so we expect the authentication dispatcher to authenticate by default.
        m_dispatcher.authenticate(
            *requestContext.connection,
            requestContext.request,
            [this, requestPath](server::AuthenticationResult result)
            {
                TestAuthenticator* authenticator = m_authenticators[requestPath].get();
                ASSERT_TRUE(authenticator);
                ASSERT_FALSE(authenticator->authenticateCalled());
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
            for (const auto& testPath : kTestPaths)
            {
                auto authenticator = std::make_unique<TestAuthenticator>();

                // Skipping kTestPaths[2]: "/test/unregisteredput"
                if (std::string(testPath) != kTestPaths[2])
                    m_dispatcher.add(std::regex(testPath), authenticator.get());

                m_authenticators.emplace(testPath, std::move(authenticator));
            }
        }

protected:
    server::AuthenticationDispatcher m_dispatcher;
    std::map<std::string, std::unique_ptr<TestAuthenticator>> m_authenticators;

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