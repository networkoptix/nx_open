// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <functional>

#include <QtCore/QPointer>

class NX_VMS_COMMON_API QnUpdatable
{
public:
    QnUpdatable();
    virtual ~QnUpdatable();

    void beginUpdate();
    void endUpdate();

protected:
    bool isUpdating() const;

    /** Called in each beginUpdate(). isUpdating() is true.  */
    virtual void beginUpdateInternal();

    /** Called in each endUpdate(). isUpdating is true. */
    virtual void endUpdateInternal();

    /** Called in first beginUpdate(). isUpdating is false. */
    virtual void beforeUpdate();

    /** Called in last endUpdate(). isUpdating is false. */
    virtual void afterUpdate();
private:
    std::atomic_int m_updateCount;
};


/** Utility class to implement RAII pattern with Updatable instances (requires QObject). */
template<typename Updatable>
class QnUpdatableGuard
{
public:
    QnUpdatableGuard(Updatable* source):
        m_guard(source)
    {
        if (m_guard)
            m_guard->beginUpdate();
    }

    virtual ~QnUpdatableGuard()
    {
        if (m_guard)
            m_guard->endUpdate();
    }

private:
    QPointer<Updatable> m_guard;
};

namespace Qn
{

template<class Updatable>
static void updateGuarded(Updatable* updatable, std::function<void()> handler)
{
    QnUpdatableGuard<Updatable> updateGuard(updatable);
    handler();
}

} // namespace Qn
