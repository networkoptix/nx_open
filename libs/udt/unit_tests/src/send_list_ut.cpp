#include <condition_variable>
#include <future>
#include <mutex>

#include <gtest/gtest.h>

#include <udt/core.h>
#include <udt/multiplexer.h>

namespace test {

class CSndUList:
    public ::testing::Test
{
public:
    CSndUList()
    {
        m_multiplexer = std::make_shared<Multiplexer>(
            AF_INET,
            /*payloadSize*/ 4*1024,
            /*mss*/ 64*1024,
            /*reuseAddr*/ true,
            /*id*/ 1);
    }

    void createUdt()
    {
        m_u = std::make_shared<CUDT>();
        m_u->open();
        m_u->setMultiplexer(m_multiplexer);
    }

protected:
//private:
    std::shared_ptr<CUDT> m_u;
    std::shared_ptr<Multiplexer> m_multiplexer;
};

TEST_F(CSndUList, no_recursive_lock_when_CSndUList_gets_CUDT_ownership)
{
    constexpr int iteratorCount = 101;

    for (int i = 0; i < iteratorCount; ++i)
    {
        createUdt();
        m_multiplexer->sendQueue().sndUList().update(m_u, true);

        std::promise<void> threadStarted;
        std::thread popThread(
            [this, &threadStarted]()
            {
                threadStarted.set_value();

                detail::SocketAddress addr;
                CPacket pkt;
                m_multiplexer->sendQueue().sndUList().pop(addr, pkt);
            });

        threadStarted.get_future().wait();

        // NOTE: Trying to acheive the situation when sndUList().pop() owns the m_u object.
        // (it locks weak_ptr inside).
        m_u.reset();

        popThread.join();
    }
}

} // namespace
