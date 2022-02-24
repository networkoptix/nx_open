// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <nx/vms/client/desktop/radass/utils/abstract_timers.h>
#include <nx/vms/client/desktop/test_support/test_timers.h>

#include <gtest/gtest.h>

namespace nx::vms::client::desktop {
namespace test {

using namespace std::chrono_literals;

class TimerReceiver: public QObject
{
public:
    void fire()
    {
        m_fired = true;
        m_fireCount ++;
    }

    bool fired() const
    {
        return m_fired;
    }

    int count()
    {
        return m_fireCount;
    }

private:
    bool m_fired = false;
    int  m_fireCount = 0;
};

TEST(TestTimersTest, BasicTimerTest)
{
    TestTimerFactory timerFactory;

    TimerPtr timer1(timerFactory.createTimer());
    TimerPtr timer2(timerFactory.createTimer());

    QSharedPointer<TimerReceiver> receiver1(new TimerReceiver());
    QSharedPointer<TimerReceiver> receiver2(new TimerReceiver());

    QObject::connect(timer1.data(), &AbstractTimer::timeout, receiver1.data(), &TimerReceiver::fire);
    QObject::connect(timer2.data(), &AbstractTimer::timeout, receiver2.data(), &TimerReceiver::fire);

    timer1->setInterval(20ms);
    timer1->start();
    timer2->start(10ms);

    timerFactory.advanceToTime(5ms);
    ASSERT_FALSE(receiver1->fired());
    ASSERT_FALSE(receiver2->fired());
    ASSERT_EQ(timerFactory.currentTime(), 5ms);

    timerFactory.passTime(6ms);
    ASSERT_FALSE(receiver1->fired());
    ASSERT_TRUE(receiver2->fired());
    ASSERT_EQ(timerFactory.currentTime(), 11ms);

    timerFactory.advanceToNextEvent();
    ASSERT_EQ(receiver1->count(), 1);
    ASSERT_EQ(receiver2->count(), 2);
    ASSERT_EQ(timerFactory.currentTime(), 20ms);

    timer2->stop();

    timerFactory.advanceToTime(60ms);
    ASSERT_EQ(receiver1->count(), 3);
    ASSERT_EQ(receiver2->count(), 2);
    ASSERT_EQ(timerFactory.currentTime(), 60ms);

    timer2->start();

    timerFactory.advanceToTime(80ms);
    ASSERT_EQ(receiver1->count(), 4);
    ASSERT_EQ(receiver2->count(), 4);
    ASSERT_EQ(timerFactory.currentTime(), 80ms);
}


TEST(TestTimersTest, BasicElapsedTimerTest)
{
    TestTimerFactory timerFactory;

    ElapsedTimerPtr timer1(timerFactory.createElapsedTimer());
    ElapsedTimerPtr timer2(timerFactory.createElapsedTimer());

    ASSERT_FALSE(timer1->isValid());
    timer1->start();
    ASSERT_TRUE(timer1->isValid());

    timerFactory.advanceToTime(5ms);
    timer2->start();

    timerFactory.passTime(5ms);

    ASSERT_TRUE(timer1->hasExpired(8ms));
    ASSERT_FALSE(timer2->hasExpired(8ms));
    ASSERT_EQ(timerFactory.currentTime(), 10ms);

    timer2->restart();

    timerFactory.passTime(10ms);
    ASSERT_TRUE(timer2->hasExpired(8ms));
    ASSERT_FALSE(timer2->hasExpired(15ms));
    ASSERT_TRUE(timer1->hasExpired(19ms));
}

} // namespace test
} // namespace nx::vms::client::desktop
