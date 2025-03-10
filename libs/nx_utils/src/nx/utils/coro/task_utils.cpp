// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "task_utils.h"

#include <QtCore/QMetaObject>
#include <QtCore/QMetaProperty>

namespace nx::coro {

nx::coro::Task<QVariant> whenProperty(
    QObject* object,
    const char* propertyName,
    std::function<bool(const QVariant&)> condition)
{
    struct Awaiter
    {
        Awaiter(QObject* object, int propertyIndex, std::function<bool(const QVariant&)> condition):
            m_object(object),
            m_index(propertyIndex),
            m_condition(condition)
        {
        }

        bool await_ready() const
        {
            return false;
        }

        void await_suspend(std::coroutine_handle<> h)
        {
            auto listener = new detail::ListenHelper();

            auto g = nx::utils::ScopeGuard(
                [this, listener = QPointer(listener), h]
                {
                    if (listener)
                    {
                        m_object->disconnect(listener);
                        listener->deleteLater();
                    }
                    m_object = {};
                    h.resume();
                });

            listener->setCallback(
                [this, h, listener, g = std::move(g)]() mutable
                {
                    m_value = m_object->metaObject()->property(m_index).read(m_object);

                    if (m_condition(m_value))
                    {
                        g.disarm();
                        m_object->disconnect(listener);
                        listener->deleteLater();
                        h.resume();
                    }
                });

            QMetaObject::connect(
                m_object,
                m_object->metaObject()->property(m_index).notifySignal().methodIndex(),
                listener,
                listener->metaObject()->indexOfMethod("execute()"));
        }

        QVariant await_resume()
        {
            if (!m_object)
                throw nx::coro::TaskCancelException();
            return m_value;
        }

        QPointer<QObject> m_object;
        int const m_index;
        std::function<bool(const QVariant&)> m_condition;
        QVariant m_value;
    };

    if (!object)
        throw nx::coro::TaskCancelException();

    const int i = object->metaObject()->indexOfProperty(propertyName);
    NX_ASSERT(i >= 0, "Property \"%1\" not found", propertyName);

    auto property = object->metaObject()->property(i);
    NX_ASSERT(property.hasNotifySignal(), "Property \"%1\" must have notify signal", propertyName);

    auto value = object->metaObject()->property(i).read(object);

    if (condition(value))
        co_return value;

    co_return co_await Awaiter(object, i, condition);
}

} // namespace nx::coro
