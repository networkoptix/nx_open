#include <gtest/gtest.h>

#include <nx/network/aio/aio_service.h>
#include <nx/network/aio/async_operation_wrapper.h>
#include <nx/network/socket_global.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx::network::aio::test {

class AsyncOperationWrapper:
    public ::testing::Test
{
public:
    ~AsyncOperationWrapper()
    {
        for (auto& context: m_wrappedOperations)
            context.operation->pleaseStopSync();
    }

protected:
    void enableTimeout(std::chrono::milliseconds timeout)
    {
        m_timeout = timeout;
    }

    void givenStartedOperation()
    {
        m_wrappedOperations.push_back(OperationContext());

        m_wrappedOperations.back().operation =
            std::make_unique<WrappedOperation>(
                [this, context = &m_wrappedOperations.back()](
                    CompletionHandler handler)
                {
                    startAsyncOperation(context, std::move(handler));
                });

        const auto operation = m_wrappedOperations.back().operation.get();
        m_wrappedOperations.back().operation->invokeWithTimeout(
            [this, operation]() { saveResult(operation); },
            m_timeout,
            [this, operation]() { saveResult(operation, /*isTimedOut*/ true); });
    }

    void givenMultipleOperations()
    {
        constexpr int kOperationCount = 17;

        for (int i = 0; i < kOperationCount; ++i)
            givenStartedOperation();
    }

    void whenOperationHasCompleted()
    {
        m_wrappedOperations.back().completionHandler();
    }

    void whenOperationsHaveCompleted()
    {
        for (auto& operationContext: m_wrappedOperations)
            operationContext.completionHandler();
    }

    void thenCompletionHandlerIsCalled()
    {
        m_prevResult = m_completedOperations.pop();
    }

    void thenEveryOperationHasCompletedAsExpected(
        bool timedOutExpected = false)
    {
        for (std::size_t i = 0; i < m_wrappedOperations.size(); ++i)
        {
            thenCompletionHandlerIsCalled();
            andCompletionHandlerIsInvokedInAioThread();
            if (timedOutExpected)
                andOperationTimedOut();
            else
                andOperationDidNotTimeOut();
        }
    }

    void thenEveryOperationHasTimedOut()
    {
        thenEveryOperationHasCompletedAsExpected(/*timedOutExpected*/ true);
    }

    void thenOperationCompletionIsReported()
    {
        thenCompletionHandlerIsCalled();
        andCompletionHandlerIsInvokedInAioThread();
        andOperationDidNotTimeOut();
    }

    void andCompletionHandlerIsInvokedInAioThread()
    {
        ASSERT_EQ(
            m_prevResult.operation->getAioThread(),
            m_prevResult.completionThread);
    }

    void andOperationTimedOut()
    {
        ASSERT_TRUE(m_prevResult.timedOut);
    }
    
    void andOperationDidNotTimeOut()
    {
        ASSERT_FALSE(m_prevResult.timedOut);
    }

private:
    using CompletionHandler = std::function<void()>;
    using WrappedOperation =
        aio::AsyncOperationWrapper<std::function<void(CompletionHandler)>>;

    struct OperationContext
    {
        std::unique_ptr<WrappedOperation> operation;
        CompletionHandler completionHandler;
    };

    struct Result
    {
        bool timedOut = false;
        aio::AbstractAioThread* completionThread = nullptr;
        WrappedOperation* operation = nullptr;
    };

    std::vector<OperationContext> m_wrappedOperations;
    nx::utils::SyncQueue<Result> m_completedOperations;
    Result m_prevResult;
    std::chrono::milliseconds m_timeout = std::chrono::milliseconds::zero();

    void startAsyncOperation(
        OperationContext* context,
        CompletionHandler completionHandler)
    {
        context->completionHandler = std::move(completionHandler);
    }

    void saveResult(
        WrappedOperation* operation,
        bool isTimedOut = false)
    {
        Result result;
        result.operation = operation;
        result.completionThread =
            nx::network::SocketGlobals::aioService().getCurrentAioThread();
        result.timedOut = isTimedOut;

        m_completedOperations.push(std::move(result));
    }
};

TEST_F(AsyncOperationWrapper, completion_handler_is_invoked)
{
    givenStartedOperation();

    whenOperationHasCompleted();

    thenCompletionHandlerIsCalled();
    andCompletionHandlerIsInvokedInAioThread();
}

TEST_F(AsyncOperationWrapper, operation_timeout)
{
    enableTimeout(std::chrono::milliseconds(1));

    givenStartedOperation();

    thenCompletionHandlerIsCalled();
    andCompletionHandlerIsInvokedInAioThread();
    andOperationTimedOut();
}

TEST_F(AsyncOperationWrapper, operation_completes_before_timeout)
{
    enableTimeout(std::chrono::hours(1));

    givenStartedOperation();
    whenOperationHasCompleted();
    thenOperationCompletionIsReported();
}

TEST_F(AsyncOperationWrapper, concurrent_operations_are_supported)
{
    givenMultipleOperations();
    whenOperationsHaveCompleted();
    thenEveryOperationHasCompletedAsExpected();
}

TEST_F(AsyncOperationWrapper, concurrent_operations_timeout)
{
    enableTimeout(std::chrono::milliseconds(1));

    givenMultipleOperations();
    thenEveryOperationHasTimedOut();
}

} // namespace nx::network::aio::test
