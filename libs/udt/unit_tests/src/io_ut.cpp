#include "test_fixture.h"

#include <thread>

#include <nx/utils/thread/sync_queue.h>

namespace udt::test {

class Io:
    public BasicFixture
{
    using base_type = BasicFixture;

public:
    Io()
    {
        initializeUdt();
    }

    ~Io()
    {
        if (m_recvThread.joinable())
            m_recvThread.join();

        deinitializeUdt();
    }

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        givenListeningServerSocket();
    }

    virtual void TearDown() override
    {
        base_type::TearDown();

        closeClientSocket();
        closeServerSocket();
    }

    void givenSocketBlockedInRecv()
    {
        givenConnectedClientSocket();

        m_recvThread = std::thread(
            [this]()
            {
                m_recvResultQueue.push(readClientSocketSync());
            });
    }

    void givenThreadThatIssuesMultipleRecv()
    {
        givenConnectedClientSocket();
        setRecvTimeout(std::chrono::milliseconds(1));

        m_recvThread = std::thread(
            [this]()
            {
                for (int i = 0; i < 51; ++i)
                    m_recvResultQueue.push(readClientSocketSync());
            });
    }

    void whenCloseSocket()
    {
        closeClientSocket();
    }

    void whenSendData()
    {
        m_data.resize(129);
        std::generate(
            m_data.begin(), m_data.end(),
            []() { return (char)(rand() % 128); });

        whenClientSends(m_data);
    }

    void thenRecvCompleted()
    {
        m_recvResultQueue.pop();
    }

    void thenDataIsReceivedOnTheOtherSide()
    {
        thenServerReceives(m_data);
    }

private:
    std::thread m_recvThread;
    nx::utils::SyncQueue<int> m_recvResultQueue;
    std::string m_data;
};

TEST_F(Io, ping)
{
    givenTwoConnectedSockets();
    whenSendData();
    thenDataIsReceivedOnTheOtherSide();
}

TEST_F(Io, closing_socket_while_reading_in_another_thread_is_supported)
{
    givenThreadThatIssuesMultipleRecv();
    whenCloseSocket();
    //thenRecvCompleted();
}

TEST_F(Io, shutdown_interrupts_recv)
{
    givenSocketBlockedInRecv();
    whenShutdownClientSocket();
    thenRecvCompleted();
}

} // namespace udt::test
