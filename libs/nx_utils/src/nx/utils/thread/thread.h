#pragma once

#include <atomic>

#include <QtCore/QThread>

#include <nx/utils/safe_direct_connection.h>

#include "semaphore.h"
#include "stoppable.h"

namespace nx {
namespace utils {

class NX_UTILS_API Thread:
    public QThread,
    public QnStoppable,
    public Qn::EnableSafeDirectConnection
{
    Q_OBJECT

public:
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
    uintptr_t systemThreadId() const;

    /**
     * @return Thread id of current thread. 
     * On unix uses gettid function instead of pthread_self.
     * It allows to find thread in gdb.
     */
    static uintptr_t currentThreadSystemId();

signals:
    void paused();

public:
    virtual void start(Priority priority = InheritPriority);
    virtual void pleaseStop() override;
    virtual void stop();

protected:
    void initSystemThreadId();

protected:
    virtual void at_started();
    virtual void at_finished();

protected:
    std::atomic<bool> m_needStop = false;
    std::atomic<bool> m_onPause = false;
    QnSemaphore m_semaphore;
    uintptr_t m_systemThreadId = 0;
    #if defined(_DEBUG)
        const std::type_info* m_type = nullptr;
    #endif
};

} // namespace utils
} // namespace nx
