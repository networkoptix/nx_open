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

class TestAuthenticator: public AbstractAuthenticationManager
{
public:
    void authenticate(
        const nx::network::http::HttpServerConnection&,
        const nx::network::http::Request&,
        nx::network::http::server::AuthenticationCompletionHandler completionHandler) override
    {
        m_authenticateCalled = true;
        completionHandler(nx::network::http::server::SuccessfulAuthenticationResult());
    }

    bool authenticateCalled() const
    {
        return m_authenticateCalled;
    }

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

      virtual int recv(void*, unsigned int, int) override
      {
          return 0;
      }

      virtual int send(const void*, unsigned int) override
      {
          return 0;
      }

      virtual bool isConnected() const override
      {
          return true;
      }
};

class TestConnectionHolder
    : public nx::network::server::StreamConnectionHolder<HttpServerConnection>
{
    virtual void closeConnection(SystemError::ErrorCode, ConnectionType*) override
    {
    }
};

} // namespace

class AuthenticationDispatcher:
    public ::testing::Test
{
protected:
    void SetUp() override
    {
        initializeAndRegisterAuthenticators();
    }

    void whenAuthenticateAllRegisteredAuthenticators()
    {
        // "/test/get"
        authenticateRegisteredPath(kTestPaths[0]);

        // "/test/delete"
        authenticateRegisteredPath(kTestPaths[1]);
    }

    void whenAuthenticateUnregisteredAuthenticator()
    {
        // "/test/unregisteredput"
        authenticateUnregisteredPath(kTestPaths[2]);
    }

private:
    void authenticateRegisteredPath(const char* path)
    {
        auto authenticator = m_authenticators[path].get();
        ASSERT_FALSE(authenticator->authenticateCalled());

        auto serverConnection = createHttpServerConnection(authenticator);
        auto httpRequest = createHttpRequest(path);

        m_dispatcher.authenticate(
            *serverConnection,
            httpRequest,
            [this, authenticator](AuthenticationResult /*result*/)
            {
                ASSERT_TRUE(authenticator->authenticateCalled());
            });
    }

    void authenticateUnregisteredPath(const char* path)
    {
        auto authenticator = m_authenticators[path].get();
        ASSERT_FALSE(authenticator->authenticateCalled());

        auto serverConnection = createHttpServerConnection(authenticator);
        auto httpRequest = createHttpRequest(path);

        m_dispatcher.authenticate(
            *serverConnection,
            httpRequest,
            [this, authenticator](AuthenticationResult /*result*/)
            {
                ASSERT_FALSE(authenticator->authenticateCalled());
            });
    }

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

    std::shared_ptr<HttpServerConnection> createHttpServerConnection(
        AbstractAuthenticationManager* authenticator)
    {
        return std::make_shared<HttpServerConnection>(
            &m_connectionHolder,
            std::make_unique<TestSocket>(),
            authenticator,
            &m_messageDispatcher);
    }

    http::Request createHttpRequest(const char* path)
    {
        http::Request request;
        request.requestLine.url.setPath(path);
        return request;
    }

private:
    server::AuthenticationDispatcher m_dispatcher;
    std::map<QString, std::unique_ptr<TestAuthenticator>> m_authenticators;

    //Needed to create HttpServerConnection object passed into authenticate()
    TestConnectionHolder m_connectionHolder;
    MessageDispatcher m_messageDispatcher;
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