#include "test_fixture.h"

namespace udt::test {

void BasicFixture::initializeUdt()
{
    UDT::startup();
}

void BasicFixture::deinitializeUdt()
{
    UDT::cleanup();
}

void BasicFixture::givenListeningServerSocket()
{
    m_serverSocket = UDT::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GT(m_serverSocket, 0);

    struct sockaddr_in localAddress;
    memset(&localAddress, 0, sizeof(localAddress));
    localAddress.sin_family = AF_INET;
    localAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
    ASSERT_EQ(
        0,
        UDT::bind(m_serverSocket, (struct sockaddr*)&localAddress, sizeof(localAddress)));

    ASSERT_EQ(0, UDT::listen(m_serverSocket, 127));

    memset(&m_serverAddress, 0, sizeof(m_serverAddress));
    int len = sizeof(m_serverAddress);
    ASSERT_EQ(0, UDT::getsockname(m_serverSocket, (struct sockaddr*) &m_serverAddress, &len));
}

void BasicFixture::startAcceptingAsync()
{
    bool blocking = false;
    ASSERT_EQ(0, UDT::setsockopt(m_serverSocket, 0, UDT_SNDSYN, &blocking, sizeof(blocking)));
    ASSERT_EQ(0, UDT::setsockopt(m_serverSocket, 0, UDT_RCVSYN, &blocking, sizeof(blocking)));

    struct sockaddr_in acceptedAddress;
    memset(&acceptedAddress, 0, sizeof(acceptedAddress));
    int len = sizeof(acceptedAddress);
    UDT::accept(m_serverSocket, (struct sockaddr*) &acceptedAddress, &len);
}

const struct sockaddr_in& BasicFixture::serverAddress() const
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
    m_clientSocket = UDT::socket(AF_INET, SOCK_STREAM, 0);

    bool blocking = false;
    ASSERT_EQ(0, UDT::setsockopt(m_clientSocket, 0, UDT_SNDSYN, &blocking, sizeof(blocking)));
    ASSERT_EQ(0, UDT::setsockopt(m_clientSocket, 0, UDT_RCVSYN, &blocking, sizeof(blocking)));

    connectToServer();
}

void BasicFixture::givenConnectedClientSocket()
{
    m_clientSocket = UDT::socket(AF_INET, SOCK_STREAM, 0);

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

void BasicFixture::whenShutdownClientSocket()
{
    ASSERT_EQ(0, UDT::shutdown(m_clientSocket, UDT_SHUT_RDWR));
}

void BasicFixture::closeClientSocket()
{
    UDT::close(m_clientSocket);
    m_clientSocket = -1;
}

void BasicFixture::connectToServer()
{
    const auto& serverAddr = serverAddress();
    ASSERT_EQ(
        0,
        UDT::connect(m_clientSocket, (const sockaddr*)&serverAddr, sizeof(serverAddr)));
}

} // namespace udt::test
