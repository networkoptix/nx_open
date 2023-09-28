// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <nx/network/socks5/messages.h>
#include <nx/network/socks5/server.h>

#include <nx/network/system_socket.h>

namespace nx::network::socks5::test {

class MockConnector: public nx::network::socks5::AbstractTunnelConnector
{
public:
    MOCK_METHOD(bool, hasAuth, (), (const));
    MOCK_METHOD(bool, auth, (const std::string&, const std::string&));
    MOCK_METHOD(void, connectTo, (const SocketAddress&, DoneCallback));
    MOCK_METHOD(void, constructorCall, ());
};

/** Redirects calls to MockConnector, so FakeConnector can be put inside unique_ptr. */
class FakeConnector: public nx::network::socks5::AbstractTunnelConnector
{
public:
    FakeConnector(MockConnector* connector): m_connector(connector)
    {
        m_connector->constructorCall();
    }

    virtual bool hasAuth() const override { return m_connector->hasAuth(); }

    virtual bool auth(const std::string& user, const std::string& password) override
    {
        return m_connector->auth(user, password);
    }

    virtual void connectTo(const SocketAddress& address, DoneCallback onDone) override
    {
        m_connector->connectTo(address, std::move(onDone));
    }

private:
    MockConnector* m_connector;
};

class Socks5ProxyServer: public ::testing::Test
{
protected:
    static constexpr auto kTestUser = "user";
    static constexpr auto kTestPassword = "testpassword";
    static constexpr auto kTestAnotherPassword = "wrongpassword";
    static constexpr auto kTestDomain = "my.domain";
    static constexpr uint16_t kTestPort = 0xabcd;

    virtual void SetUp() override
    {
        startProxyServer();
    }

    virtual void TearDown() override
    {
        if (m_client)
            m_client->pleaseStopSync();
        m_proxyServer->pleaseStopSync();
    }

    void whenClientConnected(int overallConnectionsExpected = 1)
    {
        EXPECT_CALL(*m_mockConnector, constructorCall).Times(overallConnectionsExpected);

        m_client = std::make_unique<TCPSocket>(AF_INET);

        ASSERT_TRUE(m_client->connect(m_proxyServer->address(), nx::network::kNoTimeout))
            << SystemError::getLastOSErrorText();

        ASSERT_TRUE(m_client->isConnected());
    }

    void whenRequestSuccessfullySent(const Message& message)
    {
        const auto buffer = message.toBuffer();
        const auto bytesSent = m_client->send(buffer.data(), buffer.size());
        ASSERT_EQ(bytesSent, buffer.size());
    }

    void whenGreetNoAuthSupportedSent()
    {
        whenRequestSuccessfullySent(GreetRequest({ kMethodNoAuth }));
    }

    void whenGreetUserPassAuthSupportedSent()
    {
        whenRequestSuccessfullySent(GreetRequest({ kMethodNoAuth, kMethodUserPass }));
    }

    void whenUserPassAuthSucceeded(const std::string& user, const std::string& password)
    {
        whenRequestSuccessfullySent(AuthRequest(user, password));
        thenAuthSucceeded();
    }

    void whenUserPassAuthFailed(const std::string& user, const std::string& password)
    {
        whenRequestSuccessfullySent(AuthRequest(user, password));
        thenAuthFailed();
    }

    void whenConnectDomainSent()
    {
        whenRequestSuccessfullySent(ConnectRequest(kTestDomain, kTestPort));
    }

    void whenUnsupportedCommandSent()
    {
        EXPECT_CALL(*m_mockConnector, connectTo).Times(0);

        ConnectRequest request(kTestDomain, kTestPort);
        // There are 3 commands defined in the RFC:
        //   CONNECT (0x01) - only this is supported for now
        //   BIND (0x02)
        //   UDP ASSOCIATE (0x03)
        // We choose an invalid number 0xFF to ensure the command is not supported.
        request.command = 0xFF;

        whenRequestSuccessfullySent(request);
    }

    void proxyServerResponseMatched(const Message& expectedMessage)
    {
        const Buffer expected = expectedMessage.toBuffer();

        // Try to read one byte more - this way we can detect if the incoming message is larger
        // than expected.
        Buffer response(expected.size() + 1, 0);

        const auto bytesRead = m_client->recv(response.data(), response.size(), 0);
        const auto errorCode = SystemError::getLastOSErrorCode();

        EXPECT_EQ(expected.size(), bytesRead);
        EXPECT_EQ(SystemError::noError, errorCode);

        response.resize(bytesRead); //< Do not use the additional byte for comparison.

        ASSERT_EQ(expected, response);
    }

    void thenProxyServerClosedConnection()
    {
        Buffer response(1, 0);

        const auto bytesRead = m_client->recv(response.data(), response.size(), 0);
        const auto errorCode = SystemError::getLastOSErrorCode();

        EXPECT_EQ(0, bytesRead);
        EXPECT_EQ(SystemError::noError, errorCode);
    }

    void thenProxyServerSelectedNoAuth()
    {
        proxyServerResponseMatched(GreetResponse(kMethodNoAuth));
    }

    void thenProxyServerSelectedUserPassAuth()
    {
        proxyServerResponseMatched(GreetResponse(kMethodUserPass));
    }

    void thenProxyServerUnacceptableAuth()
    {
        proxyServerResponseMatched(GreetResponse(kMethodUnacceptable));
    }

    void thenAuthSucceeded()
    {
        proxyServerResponseMatched(AuthResponse(kSuccess));
    }

    void thenAuthFailed()
    {
        proxyServerResponseMatched(AuthResponse(kGeneralFailure));
    }

    void thenConnectToDomainFailedWith(uint8_t statusCode)
    {
        proxyServerResponseMatched(ConnectResponse(statusCode, kTestDomain, kTestPort));
    }

    void thenConnectToDomainSucceeded()
    {
        proxyServerResponseMatched(ConnectResponse(kSuccess, kTestDomain, kTestPort));
    }

    void givenConnectToUnknownDomainExpected(const std::string& expectedHost, uint16_t expectedPort)
    {
        EXPECT_CALL(*m_mockConnector, connectTo).Times(1);
        ON_CALL(*m_mockConnector, connectTo).WillByDefault(
            [expectedHost, expectedPort](
                const SocketAddress& address,
                AbstractTunnelConnector::DoneCallback doneCallback)
            {
                ASSERT_EQ(expectedHost, address.address.toString());
                ASSERT_EQ(expectedPort, address.port);
                doneCallback(SystemError::hostNotFound, nullptr);
            });
    }

    void givenSelfConnectExpected(const std::string& expectedHost, uint16_t expectedPort)
    {
        EXPECT_CALL(*m_mockConnector, connectTo).Times(1);
        ON_CALL(*m_mockConnector, connectTo).WillByDefault(
            [expectedHost, expectedPort, this](
                const SocketAddress& address,
                AbstractTunnelConnector::DoneCallback doneCallback)
            {
                ASSERT_EQ(expectedHost, address.address.toString());
                ASSERT_EQ(expectedPort, address.port);

                // We ignore host and port and just connect to self server for test purposes.
                // This way there is no need to spin another server.
                m_connectorSocket = std::make_unique<TCPSocket>(AF_INET);
                ASSERT_TRUE(m_connectorSocket->setNonBlockingMode(true));

                auto handler =
                    [doneCallback = std::move(doneCallback), this](SystemError::ErrorCode result)
                    {
                        ASSERT_EQ(SystemError::noError, result);
                        doneCallback(SystemError::noError, std::move(m_connectorSocket));
                    };

                m_connectorSocket->connectAsync(m_proxyServer->address(), std::move(handler));
            });
    }

    void givenProxyServerRequiresNoAuth()
    {
        EXPECT_CALL(*m_mockConnector, hasAuth).Times(1);
        ON_CALL(*m_mockConnector, hasAuth).WillByDefault(::testing::Return(false));
    }

    void givenProxyServerRequiresUserPassAuth()
    {
        EXPECT_CALL(*m_mockConnector, hasAuth).Times(1);
        ON_CALL(*m_mockConnector, hasAuth).WillByDefault(::testing::Return(true));
    }

    void givenProxyServerRequiresUserPassAuth(
        const std::string& allowedUser,
        const std::string& allowedPassword,
        int overallConnectionsExpected = 1)
    {
        EXPECT_CALL(*m_mockConnector, hasAuth).Times(overallConnectionsExpected);
        ON_CALL(*m_mockConnector, hasAuth).WillByDefault(::testing::Return(true));

        EXPECT_CALL(*m_mockConnector, auth).Times(1);
        ON_CALL(*m_mockConnector, auth).WillByDefault(
            [allowedUser, allowedPassword](const std::string& user, const std::string& password)
            {
                return user == allowedUser && password == allowedPassword;
            });
    }

    void givenEstablishedConnectionToTargetServer()
    {
        // The test establishes connection to this SOCKS5 server using the same server, so the
        // test can perform SOCKS5 handshake inside SOCKS5 tunnel.
        // This way we do not need to spin additional server for tests.

        static constexpr auto kConnectionsCount = 2;

        // Greet.
        givenProxyServerRequiresUserPassAuth(kTestUser, kTestPassword, kConnectionsCount);

        whenClientConnected(kConnectionsCount);
        whenGreetUserPassAuthSupportedSent();

        thenProxyServerSelectedUserPassAuth();

        // Send user/pass.
        whenUserPassAuthSucceeded(kTestUser, kTestPassword);

        // Connect.
        givenSelfConnectExpected(kTestDomain, kTestPort);

        whenConnectDomainSent();

        thenConnectToDomainSucceeded();
    }

    void whenSentMessageToTargetServer()
    {
        whenGreetUserPassAuthSupportedSent();
    }

    void thenResponseMessageFromTargetServerIsReceived()
    {
        thenProxyServerSelectedUserPassAuth();
    }

    void whenTargetServerClosesConnection()
    {
        whenGreetNoAuthSupportedSent();
        thenProxyServerUnacceptableAuth();
    }

    void whenAuthorizationWithNoAuthSucceeded()
    {
        givenProxyServerRequiresNoAuth();

        whenClientConnected();
        whenGreetNoAuthSupportedSent();

        thenProxyServerSelectedNoAuth();
    }

private:
    void startProxyServer()
    {
        m_mockConnector = std::make_unique<MockConnector>();
        m_proxyServer = std::make_unique<nx::network::socks5::Server>(
            [this]
            {
                return std::make_unique<FakeConnector>(m_mockConnector.get());
            });

        ASSERT_TRUE(m_proxyServer->bind(network::SocketAddress::anyPrivateAddress))
            << SystemError::getLastOSErrorText();
        ASSERT_TRUE(m_proxyServer->listen())
            << SystemError::getLastOSErrorText();
    }

    std::unique_ptr<MockConnector> m_mockConnector;
    std::unique_ptr<Server> m_proxyServer;
    std::unique_ptr<TCPSocket> m_client;

    std::unique_ptr<TCPSocket> m_connectorSocket;
};

TEST_F(Socks5ProxyServer, greet_with_unsupported_auth)
{
    // Greet.
    givenProxyServerRequiresUserPassAuth();

    whenClientConnected();
    whenGreetNoAuthSupportedSent();

    thenProxyServerUnacceptableAuth();
    thenProxyServerClosedConnection();
}

TEST_F(Socks5ProxyServer, auth_username_password_is_denied)
{
    // Greet.
    givenProxyServerRequiresUserPassAuth(kTestUser, kTestPassword);

    whenClientConnected();
    whenGreetUserPassAuthSupportedSent();

    thenProxyServerSelectedUserPassAuth();

    // Send wrong user/pass.
    whenUserPassAuthFailed(kTestUser, kTestAnotherPassword);

    thenProxyServerClosedConnection();
}

TEST_F(Socks5ProxyServer, connect_without_required_authorization_is_denied)
{
    // Greet.
    givenProxyServerRequiresUserPassAuth();

    whenClientConnected();
    whenGreetUserPassAuthSupportedSent();

    thenProxyServerSelectedUserPassAuth();

    // Connect.
    whenConnectDomainSent(); //< This should trigger auth version mismatch.

    thenProxyServerClosedConnection();
}

TEST_F(Socks5ProxyServer, connect_to_unknown_domain_failed)
{
    // Greet.
    whenAuthorizationWithNoAuthSucceeded();

    // Connect.
    givenConnectToUnknownDomainExpected(kTestDomain, kTestPort);
    whenConnectDomainSent();

    thenConnectToDomainFailedWith(kHostUnreachable);
    thenProxyServerClosedConnection();
}

TEST_F(Socks5ProxyServer, connect_with_unsupported_command_failed)
{
    // Greet.
    whenAuthorizationWithNoAuthSucceeded();

    // Connect.
    whenUnsupportedCommandSent();

    thenConnectToDomainFailedWith(kCommandNotSupported);
    thenProxyServerClosedConnection();
}

TEST_F(Socks5ProxyServer, connect_to_domain_succeeded)
{
    givenEstablishedConnectionToTargetServer(); //< We consider handshake to be tested and working.

    whenSentMessageToTargetServer();
    thenResponseMessageFromTargetServerIsReceived();
}

TEST_F(Socks5ProxyServer, proxy_closes_connection_to_client_when_server_closes_connection_to_proxy)
{
    givenEstablishedConnectionToTargetServer(); //< We consider handshake to be tested and working.

    whenTargetServerClosesConnection();
    thenProxyServerClosedConnection();
}

} // namespace nx::network::socks5::test
