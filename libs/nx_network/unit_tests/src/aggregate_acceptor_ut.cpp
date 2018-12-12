#include <gtest/gtest.h>

#include <nx/network/aggregate_acceptor.h>
#include <nx/network/test_support/acceptor_stub.h>
#include <nx/utils/random.h>

namespace nx {
namespace network {
namespace test {

// TODO: #ak Move here corresponding tests from MultipleServerSocket tests.

namespace {

class DirectAcceptorStub:
    public AcceptorStub
{
public:
    virtual void acceptAsync(AcceptCompletionHandler handler) override
    {
        handler(SystemError::timedOut, nullptr);
    }
};

} // namespace

class AggregateAcceptor:
    public ::testing::Test
{
protected:
    void givenMultipleAcceptors()
    {
        constexpr int testAcceptorCount = 7;
        for (int i = 0; i < testAcceptorCount; ++i)
            addAcceptor(std::make_unique<AcceptorStub>());
    }

    void givenDirectAcceptor()
    {
        addAcceptor(std::make_unique<DirectAcceptorStub>());
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

    void whenAcceptInitiated()
    {
        using namespace std::placeholders;

        m_aggregateAcceptor.acceptAsync(
            std::bind(&AggregateAcceptor::onAccepted, this, _1, _2));
    }

    void thenAcceptHasProvidedResult()
    {
        m_acceptResults.pop();
    }

    void thenOtherAcceptorsWereNotStarted()
    {
        for (const auto& acceptor: m_acceptorStubs)
        {
            ASSERT_FALSE(acceptor->isAsyncAcceptInProgress());
        }
    }

    void thenAcceptorIsFreed()
    {
        ASSERT_EQ(m_acceptorToRemove, m_removedAcceptorsQueue.pop());
    }

private:
    struct AcceptResult
    {
        SystemError::ErrorCode systemErrorCode;
        std::unique_ptr<AbstractStreamSocket> connection;
    };

    utils::SyncQueue<AcceptorStub*> m_removedAcceptorsQueue;
    network::AggregateAcceptor m_aggregateAcceptor;
    std::vector<AcceptorStub*> m_acceptorStubs;
    AcceptorStub* m_acceptorToRemove = nullptr;
    nx::utils::SyncQueue<AcceptResult> m_acceptResults;

    void addAcceptor(std::unique_ptr<AcceptorStub> acceptor)
    {
        acceptor->setRemovedAcceptorsQueue(&m_removedAcceptorsQueue);
        m_acceptorStubs.push_back(acceptor.get());
        m_aggregateAcceptor.add(std::move(acceptor));
    }

    void onAccepted(
        SystemError::ErrorCode systemErrorCode,
        std::unique_ptr<AbstractStreamSocket> connection)
    {
        m_acceptResults.push({systemErrorCode, std::move(connection)});
    }
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

TEST_F(AggregateAcceptor, acceptor_invokes_handler_within_acceptAsync)
{
    givenDirectAcceptor();
    givenMultipleAcceptors();

    whenAcceptInitiated();

    thenAcceptHasProvidedResult();
    thenOtherAcceptorsWereNotStarted();
}

} // namespace test
} // namespace network
} // namespace nx
