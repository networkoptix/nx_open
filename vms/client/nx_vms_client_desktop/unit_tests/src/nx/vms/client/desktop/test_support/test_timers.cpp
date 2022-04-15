// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "test_timers.h"

#include <chrono>

#include <nx/utils/log/log_main.h>

namespace nx::vms::client::desktop::test {

using std::chrono::milliseconds;

class TestTimer: public AbstractTimer
{
public:
    TestTimer(TestTimerFactory* factory, QObject* parent):
        AbstractTimer(parent),
        m_factory(factory)
    {
    }

    ~TestTimer()
    {
        m_factory->forgetTimer(this);
    }

    virtual milliseconds interval() const override
    {
        return m_interval;
    }

    virtual bool isActive() const override
    {
        return m_active;
    }

    virtual bool isSingleShot() const override
    {
        return m_singleShot;
    }

    virtual milliseconds remainingTime() const override //< We will mimic Qt behaviour.
    {
        if (!isActive())
            return milliseconds(-1);

        return qMin(milliseconds(), (m_startMoment + m_interval) - m_factory->currentTime());
    }

    virtual void setInterval(milliseconds value) override
    {
        NX_ASSERT(value >= milliseconds(0), "No negative Test Timer interval, please.");
        if (value == milliseconds(0))
            NX_WARNING(this, "Zero interval Test Timer is risky - may deadlock.");
        m_interval = value;
    }

    virtual void setSingleShot(bool singleShot) override
    {
        m_singleShot = singleShot;
    }

    // Slots
    virtual void start(std::chrono::milliseconds value) override
    {
        // Will follow QTimer behaviour even if already running.
        setInterval(value);
        start();
    }

    virtual void start() override
    {
        m_startMoment = m_factory->currentTime();
        m_active = true;
        m_factory->forgetTimer(this);
        m_factory->registerTimerEvent(m_startMoment + interval(), this);
    }

    virtual void stop() override
    {
        m_active = false;
        m_factory->forgetTimer(this);
    }

    // Method for actually firing signal, called from factory.
    void fireTimeout()
    {
        if (!isActive())
            return;

        emit timeout();
        if (!m_singleShot)
        {
            m_startMoment += interval();
            m_factory->registerTimerEvent(m_startMoment + interval(), this);
        }
        else
            m_active = false;
    }

private:
    TestTimerFactory* m_factory = nullptr;
    milliseconds m_interval = milliseconds();
    milliseconds m_startMoment = milliseconds();
    bool m_active = false;
    bool m_singleShot = false;
};

class TestElapsedTimer: public AbstractElapsedTimer
{
public:
    TestElapsedTimer(TestTimerFactory* factory):
        m_factory(factory)
    {
    }

    virtual bool hasExpired(milliseconds value) const override
    {
        if (value == milliseconds(-1) || !m_active) //< Mimics QElapsedTimer.
            return false;

        return m_factory->currentTime() >= value + m_startMoment;
    }

    virtual bool hasExpiredOrInvalid(milliseconds value) const override
    {
        return !isValid() || hasExpired(value);
    }

    virtual milliseconds restart() override
    {
        NX_ASSERT(m_active, "Undefined timer behaviour.");
        const auto elapsedTime = m_factory->currentTime() - m_startMoment;
        start();
        return elapsedTime;
    }

    virtual void invalidate() override
    {
        m_active = false;
    }

    virtual bool isValid() const override
    {
        return m_active;
    }

    virtual milliseconds elapsed() const override
    {
        NX_ASSERT(m_active, "Undefined timer behaviour.");
        return m_factory->currentTime() - m_startMoment;
    }

    virtual qint64 elapsedMs() const override
    {
        NX_ASSERT(m_active, "Undefined timer behaviour.");
        return (m_factory->currentTime() - m_startMoment).count();
    }

    virtual void start() override
    {
        m_startMoment = m_factory->currentTime();
        m_active = true;
    }

private:
    TestTimerFactory* m_factory = nullptr;
    milliseconds m_startMoment = milliseconds();
    bool m_active = false;
};

AbstractTimer* TestTimerFactory::createTimer(QObject* parent)
{
    return new TestTimer(this, parent);
}

AbstractElapsedTimer* TestTimerFactory::createElapsedTimer()
{
    return new TestElapsedTimer(this);
}

milliseconds TestTimerFactory::currentTime() const
{
    return m_currentTime;
}

bool TestTimerFactory::hasFutureEvents() const
{
    return !m_timerEvents.empty();
}

milliseconds TestTimerFactory::nextEventTime() const
{
    if (!hasFutureEvents())
        return milliseconds(-1);

    return m_timerEvents.firstKey();
}

void TestTimerFactory::passTime(milliseconds period)
{
    advanceToTime(currentTime() + period);
}

void TestTimerFactory::advanceToTime(milliseconds timePoint)
{
    NX_ASSERT(timePoint >= m_currentTime, "No time machine is available.");

    auto time = nextEventTime();
    while (hasFutureEvents() && time <= timePoint)
    {
        m_currentTime = time;
        auto timer = m_timerEvents.first();
        // Some extra safety code to make sure we delete exactly event we selected.
        m_timerEvents.erase(m_timerEvents.begin());
        timer->fireTimeout();
        time = nextEventTime();
    }
    m_currentTime = timePoint;
}

void TestTimerFactory::advanceToNextEvent()
{
    if (m_timerEvents.empty())
        return;

    advanceToTime(m_timerEvents.firstKey());
}

void TestTimerFactory::registerTimerEvent(milliseconds timePoint, TestTimer* timer)
{
    m_timerEvents.insert(timePoint, timer);
}

void TestTimerFactory::forgetTimer(TestTimer* timer)
{
    const auto keys = m_timerEvents.keys(timer);
    for (const auto& key: keys)
        m_timerEvents.remove(key);
}

} // namespace nx::vms::client::desktop::test
