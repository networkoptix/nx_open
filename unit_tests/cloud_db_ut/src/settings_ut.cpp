#include <gtest/gtest.h>

#include <nx/network/socket_factory.h>
#include <nx/network/system_socket.h>
#include <nx/utils/random.h>
#include <nx/utils/std/future.h>
#include <nx/utils/thread/sync_queue.h>

#include "functional_tests/test_setup.h"

namespace nx {
namespace cdb {
namespace test {

class Settings:
    public CdbFunctionalTest
{
public:
    void startCdb()
    {
        ASSERT_TRUE(startAndWaitUntilStarted());
    }
};

//-------------------------------------------------------------------------------------------------
// Http server settings test fixture.

class TestTcpServerSocket:
    public nx::network::TCPServerSocket
{
public:
    TestTcpServerSocket(
        nx::utils::SyncQueue<int>* backlogValuesPassedToServerSocketListen)
        :
        m_backlogValuesPassedToServerSocketListen(backlogValuesPassedToServerSocketListen)
    {
    }

    virtual bool listen(int queueLen) override
    {
        m_backlogValuesPassedToServerSocketListen->push(queueLen);
        return nx::network::TCPServerSocket::listen(queueLen);
    }

private:
    nx::utils::SyncQueue<int>* m_backlogValuesPassedToServerSocketListen;
};

class SettingsHttp:
    public Settings
{
public:
    SettingsHttp():
        m_expectedTcpBacklogSize(0)
    {
        using namespace std::placeholders;

        m_serverSocketFactoryBak = SocketFactory::setCreateStreamServerSocketFunc(
            std::bind(&SettingsHttp::serverSocketFactoryFunc, this, _1, _2));
    }

    ~SettingsHttp()
    {
        SocketFactory::setCreateStreamServerSocketFunc(
            std::move(m_serverSocketFactoryBak));
    }

protected:
    void setTcpBacklogSizeTo(int backlog)
    {
        m_expectedTcpBacklogSize = backlog;
        addArg("-http/tcpBacklogSize");
        addArg(lit("%1").arg(backlog).toLatin1().constData());
    }

    void assertTcpBacklogSizeHasBeenAppliedToServerSocket()
    {
        const int actualBacklogSizeUsed = m_backlogValuesPassedToServerSocketListen.pop();
        ASSERT_EQ(m_expectedTcpBacklogSize, actualBacklogSizeUsed);
    }

private:
    int m_expectedTcpBacklogSize;
    nx::utils::SyncQueue<int> m_backlogValuesPassedToServerSocketListen;
    SocketFactory::CreateStreamServerSocketFuncType m_serverSocketFactoryBak;

    std::unique_ptr<AbstractStreamServerSocket> serverSocketFactoryFunc(
        bool /*sslRequired*/,
        nx::network::NatTraversalSupport /*natTraversalRequired*/)
    {
        return std::make_unique<TestTcpServerSocket>(
            &m_backlogValuesPassedToServerSocketListen);
    }
};

//-------------------------------------------------------------------------------------------------
// Http server settings test cases.

TEST_F(SettingsHttp, tcpBacklogSize_applied)
{
    const int backlogSize = nx::utils::random::number<int>(100, 60000);

    setTcpBacklogSizeTo(backlogSize);
    startCdb();
    assertTcpBacklogSizeHasBeenAppliedToServerSocket();
}

} // namespace test
} // namespace cdb
} // namespace nx
