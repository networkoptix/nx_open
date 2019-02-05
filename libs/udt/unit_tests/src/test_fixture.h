#pragma once

#if !defined(_WIN32)
#   include <arpa/inet.h>
#endif

#include <chrono>

#include <gtest/gtest.h>

#include <udt/udt.h>

namespace udt::test {

class BasicFixture:
    public ::testing::Test
{
public:
    void initializeUdt();
    void deinitializeUdt();

    void givenListeningServerSocket();
    void startAcceptingAsync();
    const struct sockaddr_in& serverAddress() const;
    void closeServerSocket();

    void givenConnectingClientSocket();
    void givenConnectedClientSocket();
    void setRecvTimeout(std::chrono::milliseconds timeout);
    int readClientSocketSync();
    void whenShutdownClientSocket();
    void closeClientSocket();

private:
    UDTSOCKET m_serverSocket = -1;
    struct sockaddr_in m_serverAddress;
    UDTSOCKET m_clientSocket = -1;

    void connectToServer();
};

} // namespace udt::test
