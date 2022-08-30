// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>

#include <QtCore/QThread>

#include <nx/utils/safe_direct_connection.h>

#include "semaphore.h"
#include "stoppable.h"

namespace nx {
namespace utils {

using ThreadId = uintptr_t;

class NX_UTILS_API Thread:
    public QThread,
    public QnStoppable,
    public /*mixin*/ Qn::EnableSafeDirectConnection
{
    Q_OBJECT

public:
    /**
     * Allows to reduce the thread stack size (from the default 8 MB on Linux) to save memory on
     * low-profile devices like ARM. The value is effective for newly created threads. The value
     * 0 means the default.
     */
    static void setThreadStackSize(int value) { s_threadStackSize = value; }

    Thread();
    virtual ~Thread();

    bool needToStop() const;

    virtual void pause();
    virtual void resume();
    virtual bool isPaused() const;
    void pauseDelay();

    /**
     * @return Thread id, corresponding to this object.
     * This id is remembered in Thread::initSystemThreadId.
     */
    ThreadId systemThreadId() const;

    /**
     * @return Thread id of current thread.
     * On unix uses gettid function instead of pthread_self.
     * It allows to find thread in gdb.
     */
    static ThreadId currentThreadSystemId();

signals:
    void paused();

public:
    virtual void start(Priority priority = InheritPriority);
    virtual void pleaseStop() override;
    virtual void stop();

    using QThread::usleep;
    static void sleep(const std::chrono::milliseconds& t) { msleep((unsigned long) t.count()); }

protected:
    void initSystemThreadId();

protected:
    virtual void at_started();
    virtual void at_finished();

protected:
    std::atomic<bool> m_needStop = false;
    std::atomic<bool> m_onPause = false;
    Semaphore m_semaphore;
    ThreadId m_systemThreadId = 0;
    #if defined(_DEBUG)
        const std::type_info* m_type = nullptr;
    #endif

private:
    static int s_threadStackSize /*= 0*/;
};

} // namespace utils
} // namespace nx
