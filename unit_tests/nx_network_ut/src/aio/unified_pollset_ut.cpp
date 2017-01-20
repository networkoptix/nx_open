#include <gtest/gtest.h>

#include <nx/network/aio/aio_thread.h>
#include <nx/network/aio/pollset_wrapper.h>
#include <nx/network/aio/unified_pollset.h>
#include <nx/network/system_socket.h>
#include <nx/network/udt/udt_socket_impl.h>
#include <nx/network/udt/udt_socket.h>
#include <nx/utils/random.h>
#include <nx/utils/std/future.h>

#include "pollset_test_common.h"

namespace nx {
namespace network {
namespace aio {
namespace test {

class TestUdtEpollWrapper:
    public AbstractUdtEpollWrapper
{
public:
    virtual int epollWait(
        int /*epollFd*/,
        std::map<UDTSOCKET, int>* readReadyUdtSockets,
        std::map<UDTSOCKET, int>* writeReadyUdtSockets,
        int64_t /*timeoutMillis*/,
        std::map<AbstractSocket::SOCKET_HANDLE, int>* readReadySystemSockets,
        std::map<AbstractSocket::SOCKET_HANDLE, int>* writeReadySystemSockets) override
    {
        std::size_t numberOfEvents = 0;

        if (readReadySystemSockets)
        {
            numberOfEvents += m_readableSystemSockets.size();
            *readReadySystemSockets = m_readableSystemSockets;
        }

        if (writeReadySystemSockets)
        {
            numberOfEvents += m_writableSystemSockets.size();
            *writeReadySystemSockets = m_writableSystemSockets;
        }

        if (readReadyUdtSockets)
        {
            numberOfEvents += m_readableUdtSockets.size();
            *readReadyUdtSockets = m_readableUdtSockets;
        }

        if (writeReadyUdtSockets)
        {
            numberOfEvents += m_writableUdtSockets.size();
            *writeReadyUdtSockets = m_writableUdtSockets;
        }

        return (int)numberOfEvents;
    }

    void markAsReadable(Pollable* const socket)
    {
        if (socket->impl()->isUdtSocket)
        {
            m_readableUdtSockets.emplace(
                static_cast<UDTSocketImpl*>(socket->impl())->udtHandle, 0);
        }
        else
        {
            m_readableSystemSockets.emplace(socket->handle(), 0);
        }
    }

    void markAsWritable(Pollable* const socket)
    {
        if (socket->impl()->isUdtSocket)
        {
            m_writableUdtSockets.emplace(
                static_cast<UDTSocketImpl*>(socket->impl())->udtHandle, 0);
        }
        else
        {
            m_writableSystemSockets.emplace(socket->handle(), 0);
        }
    }

private:
    std::map<AbstractSocket::SOCKET_HANDLE, int> m_readableSystemSockets;
    std::map<AbstractSocket::SOCKET_HANDLE, int> m_writableSystemSockets;

    std::map<UDTSOCKET, int> m_readableUdtSockets;
    std::map<UDTSOCKET, int> m_writableUdtSockets;
};

class UnifiedPollSet:
    public CommonPollSetTest
{
public:
    UnifiedPollSet():
        m_epollWrapper(std::make_unique<TestUdtEpollWrapper>()),
        m_epollWrapperPtr(m_epollWrapper.get()),
        m_pollset(std::move(m_epollWrapper))
    {
        setPollset(&m_pollset);
    }

    ~UnifiedPollSet()
    {
    }

protected:
    virtual void simulateSocketEvents(int events) override
    {
        for (const auto& socket : sockets())
        {
            if (events & aio::etRead)
                m_epollWrapperPtr->markAsReadable(socket.get());
            if (events & aio::etWrite)
                m_epollWrapperPtr->markAsWritable(socket.get());
        }
    }

private:
    std::unique_ptr<TestUdtEpollWrapper> m_epollWrapper;
    TestUdtEpollWrapper* m_epollWrapperPtr;
    aio::PollSetWrapper<aio::UnifiedPollSet> m_pollset;
};

TEST(UnifiedPollSet, all_tests)
{
    UnifiedPollSet::runTests<UnifiedPollSet>();
}

} // namespace test
} // namespace aio
} // namespace network
} // namespace nx
