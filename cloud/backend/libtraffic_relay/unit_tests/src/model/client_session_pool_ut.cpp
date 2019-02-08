#include <thread>

#include <gtest/gtest.h>

#include <nx/cloud/relay/model/client_session_pool.h>
#include <nx/cloud/relay/settings.h>

namespace nx {
namespace cloud {
namespace relay {
namespace model {
namespace test {

static const std::string kTestPeerName = "test peer name";

class ClientSessionPool:
    public ::testing::Test
{
public:
    ClientSessionPool():
        m_pool(m_settings)
    {
    }

protected:
    void addSession()
    {
        m_sessionId = QnUuid::createUuid().toSimpleString().toStdString();
        m_pool.addSession(m_sessionId, kTestPeerName);
    }

    void waitForTimeoutToExpire()
    {
        while (!m_pool.getPeerNameBySessionId(m_sessionId).empty())
            std::this_thread::yield();
    }

    void assertSessionHasBeenAdded()
    {
        ASSERT_EQ(kTestPeerName, m_pool.getPeerNameBySessionId(m_sessionId));
    }

    void assertSessionHasBeenRemoved()
    {
        ASSERT_TRUE(m_pool.getPeerNameBySessionId(m_sessionId).empty());
    }

    void assertNothingIsReturnedWhenUsingUnknownId()
    {
        ASSERT_TRUE(m_pool.getPeerNameBySessionId("unknown_session_id").empty());
    }

    void setSessionExpirationTimeout(std::chrono::milliseconds timeout)
    {
        std::ostringstream stringStream;
        stringStream << "--connectingPeer/connectSessionIdleTimeout="<<timeout.count()<<"ms";
        const auto timeoutStr = stringStream.str();

        std::array<const char*, 1> argv{
            timeoutStr.c_str()
        };

        m_settings.load(static_cast<int>(argv.size()), argv.data());
    }

private:
    conf::Settings m_settings;
    model::ClientSessionPool m_pool;
    std::string m_sessionId;
};

TEST_F(ClientSessionPool, adding_session)
{
    addSession();
    assertSessionHasBeenAdded();
}

TEST_F(ClientSessionPool, get_unknown_session_id)
{
    assertNothingIsReturnedWhenUsingUnknownId();
}

TEST_F(ClientSessionPool, session_expiration)
{
    setSessionExpirationTimeout(std::chrono::milliseconds(1));

    addSession();
    waitForTimeoutToExpire();
    assertSessionHasBeenRemoved();
}

} // namespace test
} // namespace model
} // namespace relay
} // namespace cloud
} // namespace nx
