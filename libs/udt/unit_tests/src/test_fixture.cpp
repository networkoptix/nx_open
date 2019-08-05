#include "test_fixture.h"

#ifdef _WIN32
#include <Ws2ipdef.h>
#endif

#include <gtest/gtest.h>

namespace udt::test {

void BasicFixture::initializeUdt()
{
    UDT::startup();
}

void BasicFixture::deinitializeUdt()
{
    UDT::cleanup();
}

void BasicFixture::setIpVersion(int ipVersion)
{
    m_ipVersion = ipVersion;
}

void BasicFixture::givenListeningServerSocket()
{
    m_serverSocket = UDT::socket(m_ipVersion, SOCK_STREAM, 0);
    ASSERT_GT(m_serverSocket, 0);

    detail::SocketAddress localAddress;
    if (m_ipVersion == AF_INET)
    {
        localAddress.setFamily(m_ipVersion);
        localAddress.v4().sin_addr.s_addr = inet_addr("127.0.0.1");
    }
    else
    {
        localAddress.setFamily(m_ipVersion);
        localAddress.v6().sin6_addr = in6addr_loopback;
    }

    ASSERT_EQ(0, UDT::bind(m_serverSocket, localAddress.get(), localAddress.size()));
    ASSERT_EQ(0, UDT::listen(m_serverSocket, 127));
    ASSERT_EQ(0, UDT::getsockname(m_serverSocket, m_serverAddress.get(), (int*) &m_serverAddress.length()));
}

void BasicFixture::startAcceptingAsync()
{
    enableNonBlockingMode(m_serverSocket);

    struct sockaddr_in acceptedAddress;
    memset(&acceptedAddress, 0, sizeof(acceptedAddress));
    int len = sizeof(acceptedAddress);
    UDT::accept(m_serverSocket, (struct sockaddr*) &acceptedAddress, &len);
}

void BasicFixture::whenAcceptConnection()
{
    struct sockaddr_in acceptedAddress;
    memset(&acceptedAddress, 0, sizeof(acceptedAddress));
    int len = sizeof(acceptedAddress);
    m_acceptedConnection = UDT::accept(m_serverSocket, (struct sockaddr*) &acceptedAddress, &len);
}

void BasicFixture::thenConnectionIsAccepted()
{
    ASSERT_GT(m_acceptedConnection, 0);
}

void BasicFixture::thenServerReceives(const std::string& expected)
{
    std::string received;
    received.resize(expected.size());
    int bytesRead = UDT::recv(m_acceptedConnection, received.data(), received.size(), 0);
    ASSERT_GT(bytesRead, 0);
    received.resize(bytesRead);

    ASSERT_EQ(expected, received);
}

detail::SocketAddress BasicFixture::serverAddress() const
{
    return m_serverAddress;
}

void BasicFixture::closeServerSocket()
{
    UDT::close(m_serverSocket);
    m_serverSocket = -1;
}

void BasicFixture::givenConnectingClientSocket()
{
    m_clientSocket = UDT::socket(m_ipVersion, SOCK_STREAM, 0);

    enableNonBlockingMode(m_clientSocket);

    connectToServer();
}

void BasicFixture::givenConnectedClientSocket()
{
    m_clientSocket = UDT::socket(m_ipVersion, SOCK_STREAM, 0);

    connectToServer();
}

void BasicFixture::setRecvTimeout(std::chrono::milliseconds timeout)
{
    int timeoutInt = timeout.count();
    ASSERT_EQ(0, UDT::setsockopt(
        m_clientSocket, 0, UDT_RCVTIMEO, &timeoutInt, sizeof(timeoutInt)));
}

int BasicFixture::readClientSocketSync()
{
    char buf[1024];
    return UDT::recv(m_clientSocket, buf, sizeof(buf), 0);
}

void BasicFixture::whenClientSends(const std::string& data)
{
    const auto bytesSent = UDT::send(m_clientSocket, data.data(), data.size(), 0);
    ASSERT_EQ(data.size(), bytesSent);
}

void BasicFixture::whenClientSendsAsync(const std::string& data)
{
    enableNonBlockingMode(m_clientSocket);

    const auto bytesSent = UDT::send(m_clientSocket, data.data(), data.size(), 0);
    ASSERT_EQ(data.size(), bytesSent);
}

void BasicFixture::whenShutdownClientSocket()
{
    ASSERT_EQ(0, UDT::shutdown(m_clientSocket, UDT_SHUT_RDWR));
}

void BasicFixture::closeClientSocket()
{
    UDT::close(m_clientSocket);
    m_clientSocket = -1;
}

void BasicFixture::givenTwoConnectedSockets()
{
    givenConnectedClientSocket();

    whenAcceptConnection();
    thenConnectionIsAccepted();
}

void BasicFixture::connectToServer()
{
    const auto& serverAddr = serverAddress();
    ASSERT_EQ(
        0,
        UDT::connect(m_clientSocket, serverAddr.get(), serverAddr.size()));
}

void BasicFixture::enableNonBlockingMode(UDTSOCKET handle)
{
    bool blocking = false;
    ASSERT_EQ(0, UDT::setsockopt(handle, 0, UDT_SNDSYN, &blocking, sizeof(blocking)));
    ASSERT_EQ(0, UDT::setsockopt(handle, 0, UDT_RCVSYN, &blocking, sizeof(blocking)));
}

} // namespace udt::test
