#include <vector>

#include <gtest/gtest.h>

#include <nx/network/aio/aio_task_queue.h>
#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>

#include <nx/utils/scope_guard.h>

namespace nx {
namespace network {
namespace aio {
namespace detail {
namespace test {

namespace {

class ScopedIncrement
{
public:
    ScopedIncrement(std::atomic<std::size_t>* counter):
        m_counter(counter)
    {
        ++(*m_counter);
    }

    ~ScopedIncrement()
    {
        if (m_counter)
            --(*m_counter);
    }

    ScopedIncrement(ScopedIncrement&& rhs):
        m_counter(rhs.m_counter)
    {
        rhs.m_counter = nullptr;
    }

    ScopedIncrement& operator=(ScopedIncrement&& rhs)
    {
        if (m_counter)
            --(*m_counter);
        m_counter = rhs.m_counter;
        rhs.m_counter = nullptr;

        return *this;
    }

private:
    std::atomic<std::size_t>* m_counter;
};

static constexpr std::size_t kPollableCount = 7;

} // namespace

class AioTaskQueue:
    public ::testing::Test
{
public:
    AioTaskQueue():
        m_aioTaskQueue(nullptr)
    {
    }

protected:
    void givenSeveralPollables()
    {
        for (std::size_t i = 0; i < kPollableCount; ++i)
            m_pollables.emplace_back(std::make_unique<PollableContext>());
    }

    void whenPostMultipleCallsForEachPollable()
    {
        for (auto& pollableCtx: m_pollables)
            postCall(pollableCtx.get());

        // By calling this method we make posted calls ready for calling
        //  (they are moved to another internal queue actually).
        m_aioTaskQueue.processPollSetModificationQueue(TaskType::tAll);

        for (auto& pollableCtx: m_pollables)
            postCall(pollableCtx.get());
    }

    void whenCancelledCallsOfARandomPollable()
    {
        PollableContext* pollableCtx =
            m_pollables[nx::utils::random::number<std::size_t>(0, m_pollables.size()-1)].get();

        pollableCtx->cancelledTasks = m_aioTaskQueue.cancelPostedCalls(
            pollableCtx->pollable.impl()->socketSequence);
        pollableCtx->isCancelled = true;
    }

    void assertFunctorsWereNotRemovedByCancelButReturnedtoTheCaller()
    {
        for (auto& pollableCtx: m_pollables)
        {
            ASSERT_EQ(
                pollableCtx->expectedPostedCallCounter,
                pollableCtx->postedCallCounter.load());

            if (pollableCtx->isCancelled)
            {
                ASSERT_EQ(
                    pollableCtx->expectedPostedCallCounter,
                    pollableCtx->cancelledTasks.size());

                pollableCtx->cancelledTasks.clear();
                ASSERT_EQ(0U, pollableCtx->postedCallCounter.load());
            }
        }
    }

private:
    struct PollableContext
    {
        Pollable pollable;
        std::atomic<std::size_t> postedCallCounter;
        std::size_t expectedPostedCallCounter;
        std::vector<SocketAddRemoveTask> cancelledTasks;
        bool isCancelled;

        PollableContext():
            pollable(-1),
            postedCallCounter(0),
            expectedPostedCallCounter(0),
            isCancelled(false)
        {
        }
    };

    std::vector<std::unique_ptr<PollableContext>> m_pollables;
    detail::AioTaskQueue m_aioTaskQueue;

    void postCall(PollableContext* pollableContext)
    {
        m_aioTaskQueue.addTask(PostAsyncCallTask(
            &pollableContext->pollable,
            [scopedIncrement = std::make_unique<ScopedIncrement>(
                &pollableContext->postedCallCounter)]()
            {
            }));
        ++pollableContext->expectedPostedCallCounter;
    }
};

TEST_F(AioTaskQueue, posted_calls_are_not_freed_by_queue_when_cancelling)
{
    givenSeveralPollables();
    whenPostMultipleCallsForEachPollable();
    whenCancelledCallsOfARandomPollable();
    assertFunctorsWereNotRemovedByCancelButReturnedtoTheCaller();
}

} // namespace test
} // namespace detail
} // namespace aio
} // namespace network
} // namespace nx
