#include <gtest/gtest.h>

#include <nx/network/aggregate_acceptor.h>
#include <nx/network/test_support/acceptor_stub.h>
#include <nx/utils/random.h>

namespace nx {
namespace network {
namespace test {

// TODO: #ak Move here corresponding tests from MultipleServerSocket tests.

class AggregateAcceptor:
    public ::testing::Test
{
protected:
    void givenMultipleAcceptors()
    {
        constexpr int testAcceptorCount = 7;
        for (int i = 0; i < testAcceptorCount; ++i)
        {
            auto acceptor = std::make_unique<AcceptorStub>();
            acceptor->setRemovedAcceptorsQueue(&m_removedAcceptorsQueue);
            m_acceptorStubs.push_back(acceptor.get());
            m_aggregateAcceptor.add(std::move(acceptor));
        }
    }

    void whenRemovingAnyAcceptorByPointer()
    {
        m_acceptorToRemove = nx::utils::random::choice(m_acceptorStubs);
        m_aggregateAcceptor.remove(m_acceptorToRemove);
    }

    void whenRemovingAnyAcceptorByIndex()
    {
        const std::size_t index = 
            nx::utils::random::number<std::size_t>(
                0, m_acceptorStubs.size()-1);
        m_acceptorToRemove = m_acceptorStubs[index];

        m_aggregateAcceptor.removeAt(index);
    }

    void thenAcceptorIsFreed()
    {
        ASSERT_EQ(m_acceptorToRemove, m_removedAcceptorsQueue.pop());
    }

private:
    utils::SyncQueue<AcceptorStub*> m_removedAcceptorsQueue;
    network::AggregateAcceptor m_aggregateAcceptor;
    std::vector<AcceptorStub*> m_acceptorStubs;
    AcceptorStub* m_acceptorToRemove = nullptr;
};

TEST_F(AggregateAcceptor, remove_by_pointer)
{
    givenMultipleAcceptors();
    whenRemovingAnyAcceptorByPointer();
    thenAcceptorIsFreed();
}

TEST_F(AggregateAcceptor, remove_by_index)
{
    givenMultipleAcceptors();
    whenRemovingAnyAcceptorByIndex();
    thenAcceptorIsFreed();
}

} // namespace test
} // namespace network
} // namespace nx
