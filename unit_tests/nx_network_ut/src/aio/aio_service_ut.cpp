#include <memory>

#include <gtest/gtest.h>

#include <nx/network/aio/aio_service.h>
#include <nx/network/system_socket.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace network {
namespace aio {
namespace test {

class AIOService:
    public ::testing::Test,
    public AIOEventHandler
{
public:
    ~AIOService()
    {
        m_service.pleaseStopSync();
    }

protected:
    void givenSocket()
    {
        m_socket = std::make_unique<TCPSocket>(AF_INET);
    }

    void whenRandomlyAddedAndRemovedSocketMultipleTimes()
    {
        static const auto eventType = EventType::etTimedOut;
        static const int stepCount = 100;

        std::vector<int> steps;
        steps.reserve(stepCount);

        for (int i = 0; i < stepCount; ++i)
        {
            const int action = rand() % 2;
            steps.push_back(action);
            if (action == 0)
            {
                if (eventType == EventType::etTimedOut)
                    m_service.registerTimer(m_socket.get(), std::chrono::milliseconds(1), this);
                else
                    m_service.watchSocket(m_socket.get(), eventType, this);
            }
            else if (action == 1)
            {
                m_service.post(
                    m_socket.get(), 
                    [this]()
                    {
                        m_service.removeFromWatch(m_socket.get(), eventType, true);
                    });
            }
            else if (action == 2)
            {
                m_service.removeFromWatch(m_socket.get(), eventType, rand() & 1);
            }
        }
    }

private:
    aio::AIOService m_service;
    std::unique_ptr<TCPSocket> m_socket;

    virtual void eventTriggered(Pollable* /*sock*/, aio::EventType /*eventType*/) throw()
    {
    }
};

TEST_F(AIOService, socket_correctly_added_and_removed_multiple_times)
{
    givenSocket();
    whenRandomlyAddedAndRemovedSocketMultipleTimes();
    //thenProcessHasNotCrashed();
}

} // namespace test
} // namespace aio
} // namespace network
} // namespace nx
