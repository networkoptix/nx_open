#pragma once

#include <utility>

#include <QtCore/QObject>
#include <QtCore/QPointer>

template<class Callback>
class QnGuardedCallback {
public:
    QnGuardedCallback(QObject* target, const Callback& callback):
        m_guard(target),
        m_callback(callback)
    {
        NX_ASSERT(target);
    }

    template<class... Args>
    void operator()(Args&&... args) const
    {
        if (!m_guard)
            return;

        m_callback(std::forward<Args>(args)...);
    }

private:
    QPointer<QObject> m_guard;
    Callback m_callback;
};


template<class Callback>
QnGuardedCallback<Callback> guarded(QObject* target, const Callback& callback)
{
    return QnGuardedCallback<Callback>(target, callback);
}
