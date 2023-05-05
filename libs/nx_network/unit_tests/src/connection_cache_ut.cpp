// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <tuple>

#include <gtest/gtest.h>

#include <nx/network/connection_cache.h>
#include <nx/network/system_socket.h>

namespace nx::network::test {

class ConnectionCache:
    public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        ASSERT_TRUE(m_server.bind(SocketAddress::anyPrivateAddress));
        ASSERT_TRUE(m_server.listen());
    }

    std::tuple<std::unique_ptr<AbstractStreamSocket>, std::unique_ptr<AbstractStreamSocket>>
        givenEstablishedConnection()
    {
        auto client = std::make_unique<TCPSocket>(AF_INET);
        EXPECT_TRUE(client->connect(m_server.getLocalAddress(), kNoTimeout));
        EXPECT_TRUE(client->setNonBlockingMode(true));

        auto serverConn = m_server.accept();
        EXPECT_TRUE(serverConn) << SystemError::getLastOSErrorText();

        return std::make_tuple(std::move(client), std::move(serverConn));
    }

    void saveConnectionInCache(
        const SocketAddress& endpoint,
        std::unique_ptr<AbstractStreamSocket> conn)
    {
        m_cache.put({.addr = endpoint, .isTls = false}, std::move(conn));
    }

    void assertConnectionIsRemovedFromCacheEventually(const SocketAddress&)
    {
        while (m_cache.size() > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(101));
    }

    void setCacheExpirationTimeout(std::chrono::milliseconds period)
    {
        m_cache = network::ConnectionCache(period);
    }

private:
    network::ConnectionCache m_cache;
    TCPServerSocket m_server;
};

TEST_F(ConnectionCache, closed_connection_is_dropped)
{
    auto [clientConn, serverConn] = givenEstablishedConnection();
    const auto key = clientConn->getForeignAddress();
    saveConnectionInCache(key, std::move(clientConn));

    // Close connection.
    serverConn->pleaseStopSync();
    serverConn.reset();

    assertConnectionIsRemovedFromCacheEventually(key);
}

TEST_F(ConnectionCache, connection_is_closed_by_timeout)
{
    setCacheExpirationTimeout(std::chrono::milliseconds(500));

    auto [clientConn, serverConn] = givenEstablishedConnection();
    const auto key = clientConn->getForeignAddress();

    assertConnectionIsRemovedFromCacheEventually(key);
}

} // namespace nx::network::test
