// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <future>
#include <thread>

#include <gtest/gtest.h>

#include <nx/network/async_stoppable.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

namespace nx::network::test {

struct StoppableTestClass:
    public QnStoppableAsync
{
public:
    StoppableTestClass():
        m_isRunning(true)
    {
        m_thread = std::thread(
            std::bind(&StoppableTestClass::threadMain, this));
    }

    ~StoppableTestClass()
    {
        m_thread.join();
    }

    bool isRunning() const
    {
        return m_isRunning;
    }

    virtual void pleaseStop(nx::MoveOnlyFunc<void()> completionHandler) override
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        m_handler = std::move(completionHandler);
        m_condition.wakeOne();
    }

private:
    nx::Mutex m_mutex;
    bool m_isRunning;
    nx::WaitCondition m_condition;
    nx::MoveOnlyFunc< void() > m_handler;
    std::thread m_thread;

    void threadMain()
    {
        nx::MoveOnlyFunc< void() > handler;
        {
            NX_MUTEX_LOCKER lk(&m_mutex);
            while (!m_handler)
                m_condition.wait(&m_mutex);

            m_isRunning = false;
            handler.swap(m_handler);
        }

        handler();
    }
};

TEST(QnStoppableAsync, SingleAsync)
{
    StoppableTestClass s;
    std::promise< bool > p;
    s.pleaseStop([&]() { p.set_value(true); });
    ASSERT_TRUE(p.get_future().get());
    ASSERT_FALSE(s.isRunning());
}

TEST(QnStoppableAsync, SingleSync)
{
    StoppableTestClass s;
    s.pleaseStopSync();
    ASSERT_FALSE(s.isRunning());
}

TEST(QnStoppableAsync, MultiManual)
{
    StoppableTestClass s1;
    StoppableTestClass s2;
    StoppableTestClass s3;

    auto runningCount = [&]() {
        return (s1.isRunning() ? 1 : 0) +
            (s2.isRunning() ? 1 : 0) +
            (s3.isRunning() ? 1 : 0);
    };

    EXPECT_EQ(runningCount(), 3);
    s1.pleaseStopSync();
    EXPECT_EQ(runningCount(), 2);
    s2.pleaseStopSync();
    EXPECT_EQ(runningCount(), 1);
    s3.pleaseStopSync();
    EXPECT_EQ(runningCount(), 0);
}

} // namespace nx::network::test
