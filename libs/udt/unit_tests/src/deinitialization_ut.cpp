#if !defined(_WIN32)
#   include <arpa/inet.h>
#endif

#include <thread>

#include <gtest/gtest.h>

#include <udt/udt.h>

namespace udt::test {

class Deinitialization:
    public ::testing::Test
{
protected:
    void initializeUdt()
    {
        UDT::startup();
    }

    void deinitializeUdt()
    {
        UDT::cleanup();
    }

    void givenListeningServerSocket()
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

    void startAccepting()
    {
        bool blocking = false;
        ASSERT_EQ(0, UDT::setsockopt(m_serverSocket, 0, UDT_SNDSYN, &blocking, sizeof(blocking)));
        ASSERT_EQ(0, UDT::setsockopt(m_serverSocket, 0, UDT_RCVSYN, &blocking, sizeof(blocking)));

        struct sockaddr_in acceptedAddress;
        memset(&acceptedAddress, 0, sizeof(acceptedAddress));
        int len = sizeof(acceptedAddress);
        UDT::accept(m_serverSocket, (struct sockaddr*) &acceptedAddress, &len);
    }
    
    void givenConnectingClientSocket()
    {
        m_clientSocket = UDT::socket(AF_INET, SOCK_STREAM, 0);
        
        bool blocking = false;
        ASSERT_EQ(0, UDT::setsockopt(m_clientSocket, 0, UDT_SNDSYN, &blocking, sizeof(blocking)));
        ASSERT_EQ(0, UDT::setsockopt(m_clientSocket, 0, UDT_RCVSYN, &blocking, sizeof(blocking)));

        ASSERT_EQ(
            0,
            UDT::connect(m_clientSocket, (const sockaddr*) &m_serverAddress, sizeof(m_serverAddress)));
    }

    void whenRemoveAllSockets()
    {
        UDT::close(m_serverSocket);
        m_serverSocket = -1;

        UDT::close(m_clientSocket);
        m_clientSocket = -1;

        // TODO
    }

private:
    UDTSOCKET m_serverSocket = -1;
    struct sockaddr_in m_serverAddress;

    UDTSOCKET m_clientSocket = -1;
};

TEST_F(Deinitialization, does_not_crash_the_process)
{
    initializeUdt();

    givenListeningServerSocket();
    startAccepting();
    
    givenConnectingClientSocket();

    std::this_thread::sleep_for(
        std::chrono::milliseconds(rand() % 5));

    whenRemoveAllSockets();
    
    deinitializeUdt();

    // thenProcessDoesNotCrash();
}

} // namespace udt::test
