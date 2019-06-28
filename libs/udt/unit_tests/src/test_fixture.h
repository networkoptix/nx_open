#pragma once

#if !defined(_WIN32)
#   include <arpa/inet.h>
#endif

#include <chrono>

#include <gtest/gtest.h>

#include <udt/socket_addresss.h>
#include <udt/udt.h>

namespace udt::test {

class BasicFixture:
    public ::testing::Test
{
public:
    void initializeUdt();
    void deinitializeUdt();
    void setIpVersion(int ipVersion);

    void givenListeningServerSocket();
    void startAcceptingAsync();
    void whenAcceptConnection();
    void thenConnectionIsAccepted();
    void thenServerReceives(const std::string& data);
    detail::SocketAddress serverAddress() const;
    void closeServerSocket();

    void givenConnectingClientSocket();
    void givenConnectedClientSocket();
    void setRecvTimeout(std::chrono::milliseconds timeout);
    int readClientSocketSync();
    void whenClientSends(const std::string& data);
    void whenClientSendsAsync(const std::string& data);
    void whenShutdownClientSocket();
    void closeClientSocket();

    void givenTwoConnectedSockets();

private:
    int m_ipVersion = AF_INET;
    UDTSOCKET m_serverSocket = -1;
    UDTSOCKET m_acceptedConnection = -1;
    detail::SocketAddress m_serverAddress;
    UDTSOCKET m_clientSocket = -1;

    void connectToServer();

    void enableNonBlockingMode(UDTSOCKET handle);
};

} // namespace udt::test
