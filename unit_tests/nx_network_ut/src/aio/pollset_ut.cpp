#include <gtest/gtest.h>

#include <nx/network/aio/pollset.h>

#include "pollset_test_common.h"

namespace nx {
namespace network {
namespace aio {
namespace test {

class PollSet:
    public CommonPollSetTest<aio::PollSet>,
    public ::testing::Test
{
public:
    PollSet()
    {
        setPollset(&m_pollset);
    }

    ~PollSet()
    {
    }

protected:
    virtual void simulateSocketEvents(int /*events*/) override
    {
        for (const auto& socket: sockets())
        {
            const auto udpSocket = static_cast<UDPSocket*>(socket.get());

            char buf[16];
            ASSERT_TRUE(udpSocket->sendTo(buf, sizeof(buf), udpSocket->getLocalAddress())) 
                << SystemError::getLastOSErrorText().toStdString();
        }
    }
        
    virtual std::unique_ptr<Pollable> createSocketOfRandomType() override
    {
        return createRegularSocket();
    }

private:
    aio::PollSet m_pollset;
};

TEST_F(PollSet, all_tests)
{
    runTests();
}

} // namespace test
} // namespace aio
} // namespace network
} // namespace nx
