#include <atomic>

#include <gtest/gtest.h>

#include <nx/network/aio/aio_service.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/aio/pollset_factory.h>
#include <nx/network/socket_global.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace network {
namespace aio {
namespace test {

class TestPollable:
    public aio::BasicPollable
{
    using base_type = aio::BasicPollable;

public:
    TestPollable():
        m_cleanupDone(false)
    {
    }

    ~TestPollable()
    {
        if (isInSelfAioThread())
            stopWhileInAioThread();
    }

    bool isCleanupDone() const
    {
        return m_cleanupDone;
    }

protected:
    virtual void stopWhileInAioThread() override
    {
        base_type::stopWhileInAioThread();
        m_cleanupDone = true;
    }

private:
    bool m_cleanupDone;
};

class BasicPollable:
    public ::testing::Test
{
public:
    BasicPollable()
    {
    }

    ~BasicPollable()
    {
        m_pollable.pleaseStopSync();
    }

protected:
    TestPollable& pollable()
    {
        return m_pollable;
    }

    void recordAsyncCall()
    {
        m_callMadePromise.set_value(SocketGlobals::aioService().getCurrentAioThread());
    }

    void givenPollableBoundTo(AbstractAioThread* aioThread)
    {
        pollable().bindToAioThread(aioThread);
    }

    void whenPostedAsyncCall()
    {
        pollable().post([this]() { recordAsyncCall(); });
    }

    void assertIfAsyncCallHasNotBeenMade()
    {
        if (!m_callResult)
            m_callResult = m_callMadePromise.get_future().get();

        ASSERT_EQ(pollable().getAioThread(), *m_callResult);
    }

    void assertThatAsyncCallHasBeenMadeWithinCurrentThread()
    {
        thenAsyncCallShouldBeMadeWithinThread(
            SocketGlobals::aioService().getCurrentAioThread());
    }

    void thenAsyncCallShouldBeMadeWithinThread(AbstractAioThread* aioThread)
    {
        if (!m_callResult)
            m_callResult = m_callMadePromise.get_future().get();

        ASSERT_EQ(aioThread, *m_callResult);
    }

    void whenRequestedStop()
    {
        pollable().pleaseStopSync();
    }

    void thenCleanupShouldOccur()
    {
        ASSERT_TRUE(pollable().isCleanupDone());
    }

private:
    TestPollable m_pollable;
    nx::utils::promise<AbstractAioThread*> m_callMadePromise;
    boost::optional<AbstractAioThread*> m_callResult;
};

TEST_F(BasicPollable, post)
{
    whenPostedAsyncCall();

    assertIfAsyncCallHasNotBeenMade();
}

TEST_F(BasicPollable, dispatch_within_non_aio_thread)
{
    pollable().dispatch([this]() { recordAsyncCall(); });

    assertIfAsyncCallHasNotBeenMade();
}

TEST_F(BasicPollable, dispatch_within_aio_thread)
{
    pollable().post(
        [this]()
        {
            pollable().dispatch([this]() { recordAsyncCall(); });

            assertIfAsyncCallHasNotBeenMade();
            assertThatAsyncCallHasBeenMadeWithinCurrentThread();
        });
}

TEST_F(BasicPollable, bindToAioThread)
{
    const auto randomAioThread = SocketGlobals::aioService().getRandomAioThread();

    givenPollableBoundTo(randomAioThread);
    whenPostedAsyncCall();
    thenAsyncCallShouldBeMadeWithinThread(randomAioThread);
}

TEST_F(BasicPollable, getAioThread)
{
    const auto randomAioThread = SocketGlobals::aioService().getRandomAioThread();
    pollable().bindToAioThread(randomAioThread);
    ASSERT_EQ(randomAioThread, pollable().getAioThread());
}

TEST_F(BasicPollable, isInSelfAioThread)
{
    ASSERT_FALSE(pollable().isInSelfAioThread());

    nx::utils::promise<void> callMadePromise;
    pollable().post(
        [this, &callMadePromise]()
        {
            ASSERT_TRUE(pollable().isInSelfAioThread());
            callMadePromise.set_value();
        });
    callMadePromise.get_future().wait();
}

TEST_F(BasicPollable, stopWhileInAioThread)
{
    whenRequestedStop();
    thenCleanupShouldOccur();
}

//TEST_F(BasicPollable, pleaseStop)

TEST_F(BasicPollable, post_cancelled_by_pleaseStopSync_called_within_aio_thread)
{
    nx::utils::promise<void> secondCallPosted;
    std::atomic<int> asyncCallsDone(0);

    pollable().post(
        [this, &asyncCallsDone, &secondCallPosted]()
        {
            ++asyncCallsDone;
            secondCallPosted.get_future().wait();
            pollable().pleaseStopSync();
        });

    pollable().post([&asyncCallsDone]() { ++asyncCallsDone; });
    secondCallPosted.set_value();

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_EQ(1, asyncCallsDone.load());
}

//-------------------------------------------------------------------------------------------------
// PerformanceBasicPollable

template<typename Func>
class PerformanceBasicPollable:
    public BasicPollable
{
public:
    PerformanceBasicPollable(Func func):
        m_func(func)
    {
    }

protected:
    struct Result
    {
        std::chrono::milliseconds testDuration;
        std::size_t asyncCallsMade;
    };

    void givenAioServiceWithRegularPollSet()
    {
        PollSetFactory::instance()->disableUdt();
        m_customAioService = std::make_unique<aio::AIOService>();
        ASSERT_TRUE(m_customAioService->initialize());
        PollSetFactory::instance()->enableUdt();
    }

    void runTest()
    {
        constexpr auto testDuration = std::chrono::seconds(3);

        std::atomic<std::size_t> postCallCounter(0);

        std::unique_ptr<aio::BasicPollable> aioObject;
        if (m_customAioService)
            aioObject = std::make_unique<aio::BasicPollable>(m_customAioService.get(), nullptr);
        else
            aioObject = std::make_unique<aio::BasicPollable>();

        std::size_t prevCallCounter = (std::size_t)-1;
        const auto endTime = std::chrono::steady_clock::now() + testDuration;
        while (std::chrono::steady_clock::now() < endTime)
        {
            if (prevCallCounter != postCallCounter)
            {
                prevCallCounter = postCallCounter;
                (aioObject.get()->*m_func)(
                    [&postCallCounter]()
                    {
                        ++postCallCounter;
                    });
            }

            std::this_thread::yield();
        }

        aioObject->pleaseStopSync();

        m_result.testDuration = testDuration;
        m_result.asyncCallsMade = postCallCounter;
    }

    void printResult()
    {
        using namespace std::chrono;

        std::cout << "post performance. Total " << m_result.asyncCallsMade << " calls made in "
            << m_result.testDuration.count() << " ms. That gives "
            << (m_result.asyncCallsMade * 1000 / m_result.testDuration.count()) << " calls per second"
            << std::endl;
    }

private:
    Result m_result;
    std::unique_ptr<aio::AIOService> m_customAioService;
    Func m_func;
};

//-------------------------------------------------------------------------------------------------
// post

class PerformanceBasicPollablePost:
    public PerformanceBasicPollable<decltype(&aio::BasicPollable::post)>
{
public:
    PerformanceBasicPollablePost():
        PerformanceBasicPollable<decltype(&aio::BasicPollable::post)>(&aio::BasicPollable::post)
    {
    }
};

TEST_F(PerformanceBasicPollablePost, defaultPollset)
{
    runTest();
    printResult();
}

TEST_F(PerformanceBasicPollablePost, regularPollSet)
{
    givenAioServiceWithRegularPollSet();
    runTest();
    printResult();
}

//-------------------------------------------------------------------------------------------------
// dispatch

class PerformanceBasicPollableDispatch:
    public PerformanceBasicPollable<decltype(&aio::BasicPollable::dispatch)>
{
public:
    PerformanceBasicPollableDispatch():
        PerformanceBasicPollable<decltype(&aio::BasicPollable::dispatch)>(&aio::BasicPollable::dispatch)
    {
    }
};

TEST_F(PerformanceBasicPollableDispatch, defaultPollset)
{
    runTest();
    printResult();
}

TEST_F(PerformanceBasicPollableDispatch, regularPollSet)
{
    givenAioServiceWithRegularPollSet();
    runTest();
    printResult();
}

} // namespace test
} // namespace aio
} // namespace network
} // namespace nx
