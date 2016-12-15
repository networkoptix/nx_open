#include <gtest/gtest.h>

#include <nx/network/aio/aioservice.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/socket_global.h>

namespace nx {
namespace network {
namespace aio {
namespace test {

class TestPollable:
    public aio::BasicPollable
{
public:
    TestPollable():
        m_cleanupDone(false)
    {
    }

    ~TestPollable()
    {
        stopWhileInAioThread();
    }

    bool isCleanupDone() const
    {
        return m_cleanupDone;
    }

protected:
    virtual void stopWhileInAioThread() override
    {
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

TEST_F(BasicPollable, pleaseStop)
{
    // TODO
}

TEST_F(BasicPollable, pleaseStopSync)
{
    // TODO
}

} // namespace test
} // namespace aio
} // namespace network
} // namespace nx
