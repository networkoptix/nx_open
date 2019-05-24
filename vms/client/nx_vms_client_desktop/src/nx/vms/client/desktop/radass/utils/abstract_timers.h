#pragma once

#include <chrono>
#include <QtCore/QObject>
#include <QtCore/QSharedPointer>

/**
 * The purpose of this unit is to define abstract timer classes and abstract timer factory.
 * These timers can be implemented using normal Qt timers as well as dummy test timers that allow
 * unit testing of classes depending on timers.
 */

namespace nx::vms::client::desktop {

/**
* The class essentially defines interface similar to QTimer, but using std::chrono.
*/
class AbstractTimer: public QObject
{
    Q_OBJECT

public:
    using milliseconds = std::chrono::milliseconds;
    AbstractTimer(QObject* parent = nullptr) : QObject(parent) {}

    virtual milliseconds interval() const = 0;
    virtual bool isActive() const = 0;
    virtual bool isSingleShot() const = 0;
    virtual milliseconds remainingTime() const = 0;
    virtual void setInterval(milliseconds value) = 0;
    virtual void setSingleShot(bool singleShot) = 0;

    virtual void start(std::chrono::milliseconds value) = 0;
    virtual void start() = 0;
    virtual void stop() = 0;

signals:
    void timeout();
};

typedef QSharedPointer<AbstractTimer> TimerPtr;

/**
* The class essentially defines interface equivalent to QElapsedTimer, but using std::chrono.
*/
class AbstractElapsedTimer: public QObject
{
    Q_OBJECT

public:
    using milliseconds = std::chrono::milliseconds;

    AbstractElapsedTimer(QObject* parent = nullptr) : QObject(parent) {}
    virtual bool hasExpired(milliseconds value) const = 0;
    virtual milliseconds restart() = 0;
    virtual void invalidate() = 0;
    virtual bool isValid() const = 0;
    virtual milliseconds elapsed() const = 0;
    virtual qint64 elapsedMs() const = 0;
    virtual void start() = 0;
};

typedef QSharedPointer<AbstractElapsedTimer> ElapsedTimerPtr;

/**
* The class defines abstract factory for providing timers.
* There are implementations to provide Qt-based timers and
*/
class AbstractTimerFactory
{
public:
    virtual ~AbstractTimerFactory() = default;

    virtual AbstractTimer* createTimer(QObject* parent = nullptr) = 0;
    virtual AbstractElapsedTimer* createElapsedTimer(QObject* parent = nullptr) = 0;
};

typedef QSharedPointer<AbstractTimerFactory> TimerFactoryPtr;

} // namespace nx::vms::client::desktop

