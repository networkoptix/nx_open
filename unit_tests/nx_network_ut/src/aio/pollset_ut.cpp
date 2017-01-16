#include <gtest/gtest.h>

#include <nx/network/aio/pollset.h>
#include <nx/network/aio/pollset_wrapper.h>

#include "pollset_test_common.h"

namespace nx {
namespace network {
namespace aio {
namespace test {

class PollSet:
    public CommonPollSetTest
{
public:
    PollSet()
    {
        setPollset(&m_pollset);
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
    aio::PollSetWrapper<aio::PollSet> m_pollset;
};

TEST(PollSet, all_tests)
{
    PollSet::runTests<PollSet>();
}

} // namespace test
} // namespace aio
} // namespace network
} // namespace nx
