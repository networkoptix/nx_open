#pragma once

#if defined(_WIN32)
    #include <ws2tcpip.h>
#else
    #include <arpa/inet.h>
#endif

#include <chrono>

#include <gtest/gtest.h>

#include <udt/channel.h>
#include <udt/socket_addresss.h>
#include <udt/udt.h>

namespace udt::test {

class BasicFixture:
    public ::testing::Test
{
public:
    ~BasicFixture();

    void initializeUdt();
    void deinitializeUdt();

    void setIpVersion(int ipVersion);
    int ipVersion() const;

    void givenListeningServerSocket();
    void startAcceptingAsync();
    UDTSOCKET whenAcceptConnection();
    void thenConnectionIsAccepted();
    void thenServerReceives(const std::string& data);
    detail::SocketAddress serverAddress() const;
    void closeServerSocket();

    void givenClientSocket();
    void givenConnectingClientSocket();
    void givenConnectedClientSocket();
    void setRecvTimeout(std::chrono::milliseconds timeout);
    int readClientSocketSync();
    void whenClientSends(const std::string& data);
    void whenClientSendsAsync(const std::string& data);
    void whenShutdownClientSocket();
    void closeClientSocket();

    void givenTwoConnectedSockets();

    UDTSOCKET clientSocket() { return m_clientSocket; }
    UDTSOCKET serverSocket() { return m_serverSocket; }

    void installReorderingChannel();

private:
    int m_ipVersion = AF_INET;
    UDTSOCKET m_serverSocket = -1;
    UDTSOCKET m_acceptedConnection = -1;
    detail::SocketAddress m_serverAddress;
    UDTSOCKET m_clientSocket = -1;
    std::optional<UdpChannelFactory::FactoryFunc> m_channelFactoryBak;

    void connectToServer();

    void enableNonBlockingMode(UDTSOCKET handle);
};

} // namespace udt::test
