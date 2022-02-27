// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "qt_timers.h"

#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtCore/QElapsedTimer>

namespace {

using namespace nx::vms::client::desktop;

class QtBasedTimer: public AbstractTimer
{
    // Cannot use Q_OBJECT because in anonymous namespace.
public:
    QtBasedTimer(QObject* parent = nullptr): AbstractTimer(parent)
    {
        connect(&m_timer, &QTimer::timeout, this, &AbstractTimer::timeout);
    }

    virtual milliseconds interval() const override
    {
        return m_timer.intervalAsDuration();
    }

    virtual bool isActive() const override
    {
        return m_timer.isActive();
    }

    virtual bool isSingleShot() const override
    {
        return m_timer.isSingleShot();
    }

    virtual milliseconds remainingTime() const override
    {
        return m_timer.remainingTimeAsDuration();
    }

    virtual void setInterval(milliseconds value) override
    {
        m_timer.setInterval(value.count());
    }

    virtual void setSingleShot(bool singleShot) override
    {
        m_timer.setSingleShot(singleShot);
    }

    // Slots
    virtual void start(std::chrono::milliseconds value) override
    {
        m_timer.start(value.count());
    }

    virtual void start() override
    {
        m_timer.start();
    }

    virtual void stop() override
    {
        m_timer.stop();
    }

private:
    QTimer m_timer;
};

class QtBasedElapsedTimer: public AbstractElapsedTimer
{
public:
    virtual bool hasExpired(milliseconds value) const override
    {
        return m_timer.hasExpired(value.count());
    }

    virtual bool hasExpiredOrInvalid(milliseconds value) const override
    {
        return !isValid() || hasExpired(value);
    }

    virtual milliseconds restart() override
    {
        return milliseconds(m_timer.restart());
    }

    virtual void invalidate() override
    {
        m_timer.invalidate();
    }

    virtual bool isValid() const override
    {
        return m_timer.isValid();
    }

    virtual milliseconds elapsed() const override
    {
        return milliseconds(m_timer.elapsed());
    }

    virtual qint64 elapsedMs() const override
    {
        return m_timer.elapsed();
    }

    virtual void start() override
    {
        m_timer.start();
    }

private:
    QElapsedTimer m_timer;
};


} // namespace

namespace nx::vms::client::desktop {

// Timer Factory.
AbstractTimer* QtTimerFactory::createTimer(QObject* parent)
{
    return new QtBasedTimer(parent);
}

AbstractElapsedTimer* QtTimerFactory::createElapsedTimer()
{
    return new QtBasedElapsedTimer();
}

} // namespace nx::vms::client::desktop
