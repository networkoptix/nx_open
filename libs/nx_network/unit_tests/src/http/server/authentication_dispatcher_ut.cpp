// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/network/http/server/authentication_dispatcher.h>

#include <nx/network/http/http_client.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/url/url_parse_helper.h>

#include <nx/network/test_support/dummy_socket.h>

namespace nx::network::http::server::test {

namespace {

static const std::vector<const char*> kTestPaths =
{
    "/test/get",
    "/test/delete",
    "/test/unregisteredput"
};

class TestAuthenticator:
    public AbstractAuthenticationManager
{
    using base_type = AbstractAuthenticationManager;

public:
    using base_type::base_type;

    virtual void serve(
        RequestContext /*requestContext*/,
        nx::utils::MoveOnlyFunc<void(RequestResult)> completionHandler) override
    {
        m_authenticateCalled = true;
        completionHandler(nx::network::http::server::SuccessfulAuthenticationResult());
    }

    bool authenticateCalled() const
    {
        return m_authenticateCalled;
    }

    virtual void pleaseStopSync() override {}

private:
    bool m_authenticateCalled = false;
};

class TestSocket: public DummySocket
{
public:
      virtual bool close() override
      {
          return true;
      }

      virtual bool isClosed() const override
      {
          return true;
      }

      virtual bool shutdown() override
      {
          return true;
      }

      virtual int recv(void*, std::size_t, int) override
      {
          return 0;
      }

      virtual int send(const void*, std::size_t) override
      {
          return 0;
      }

      virtual bool isConnected() const override
      {
          return true;
      }
};

} // namespace

class AuthenticationDispatcher:
    public ::testing::Test
{
public:
    AuthenticationDispatcher():
        m_dispatcher(/*no default handler*/ nullptr)
    {
    }

protected:
    void SetUp() override
    {
        initializeAndRegisterAuthenticators();
    }

    void whenAuthenticateRegisteredPath()
    {
        // "/test/get"
        authenticateRegisteredPath(kTestPaths[0]);
        m_lastInvokedPath = kTestPaths[0];
    }

    void whenAuthenticateUnregisteredPath()
    {
        invokeDispatcher("/foo/bar");
    }

    void thenCorrespondingAuthenticatorIsInvoked()
    {
        const auto& authenticator = m_authenticators[m_lastInvokedPath];
        ASSERT_TRUE(authenticator->authenticateCalled());
    }

    void thenNoAuthenticatorIsInvoked()
    {
        for (const auto& [path, authenticator]: m_authenticators)
        {
            ASSERT_FALSE(authenticator->authenticateCalled());
        }
    }

    void andRequestCompletedWith(StatusCode::Value expected)
    {
        for (const auto& status: m_requestCompletionStatuses)
        {
            ASSERT_EQ(expected, status);
        }
    }

private:
    void authenticateRegisteredPath(const char* path)
    {
        auto authenticator = m_authenticators[path].get();
        ASSERT_FALSE(authenticator->authenticateCalled());

        invokeDispatcher(path);

        ASSERT_TRUE(authenticator->authenticateCalled());
    }

    void invokeDispatcher(const char* path)
    {
        RequestContext requestContext;
        requestContext.request = createHttpRequest(path);
        auto serverConnection = createHttpServerConnection();
        requestContext.conn = serverConnection;
        m_dispatcher.serve(
            std::move(requestContext),
            [this, serverConnection = std::move(serverConnection)](
                RequestResult result)
            {
                m_requestCompletionStatuses.push_back(result.statusCode);
            });
    }

    void initializeAndRegisterAuthenticators()
    {
        for (const auto& testPath: kTestPaths)
        {
            auto authenticator = std::make_unique<TestAuthenticator>(&m_dispatcher);

            // Skipping kTestPaths[2]: "/test/unregisteredput"
            if (std::string(testPath) != kTestPaths[2])
                m_dispatcher.add(std::regex(testPath), authenticator.get());

            m_authenticators.emplace(testPath, std::move(authenticator));
        }
    }

    std::shared_ptr<HttpServerConnection> createHttpServerConnection()
    {
        return std::make_shared<HttpServerConnection>(
            std::make_unique<TestSocket>(),
            &m_dispatcher);
    }

    http::Request createHttpRequest(const std::string& path)
    {
        http::Request request;
        request.requestLine.url.setPath(path);
        return request;
    }

private:
    server::AuthenticationDispatcher m_dispatcher;
    std::map<std::string, std::unique_ptr<TestAuthenticator>> m_authenticators;
    MessageDispatcher m_messageDispatcher;
    std::vector<StatusCode::Value> m_requestCompletionStatuses;
    std::string m_lastInvokedPath;
};

TEST_F(AuthenticationDispatcher, dispatches_matched_requests_to_registered_authenticator)
{
    whenAuthenticateRegisteredPath();

    thenCorrespondingAuthenticatorIsInvoked();
    andRequestCompletedWith(StatusCode::ok);
}

TEST_F(AuthenticationDispatcher, request_is_forbidden_when_no_authenticator_matched)
{
    whenAuthenticateUnregisteredPath();

    thenNoAuthenticatorIsInvoked();
    andRequestCompletedWith(StatusCode::forbidden);
}

} // namespace nx::network::http::server::test
