// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "qml_property.h"

#include <QtQml/QQmlProperty>
#include <QtQuick/QQuickView>

#include <nx/utils/log/assert.h>

namespace nx::vms::client::core {

class QmlPropertyBase::SharedState: public detail::QmlPropertyConnection
{
public:
    QPointer<QObject> target = nullptr;
    QQmlProperty property;
    ObjectHolder* objectHolder = nullptr;
    // Stores the property name when an object holder is used.
    QString propertyName;

    virtual void setTarget(QObject* object, const QString& name) override;
    void updateRootObject();
};

void QmlPropertyBase::SharedState::setTarget(QObject* object, const QString& name)
{
    if (target == object && propertyName == name)
        return;

    if (target != object)
    {
        if (target)
            target->disconnect(this);

        target = object;
    }

    propertyName = name;
    if (target)
    {
        property = QQmlProperty(object, name);
        NX_ASSERT(property.isValid(), "Not a valid property %1", name);
    }
    else
    {
        property = QQmlProperty();
    }

    if (property.hasNotifySignal())
        property.connectNotifySignal(this, SIGNAL(valueChanged()));

    emit valueChanged();
}

void QmlPropertyBase::SharedState::updateRootObject()
{
    if (!objectHolder)
        return;

    setTarget(objectHolder->object(), propertyName);
}

//-------------------------------------------------------------------------------------------------

QObject* QmlPropertyBase::ObjectHolder::object() const
{
    return m_object;
}

void QmlPropertyBase::ObjectHolder::setObject(QObject* object)
{
    if (m_object == object)
        return;

    m_object = object;
    notifyProperties();
}

void QmlPropertyBase::ObjectHolder::notifyProperties()
{
    for (auto it = m_properties.begin(); it != m_properties.end(); /*no increment*/)
    {
        if (const auto state = it->lock())
        {
            state->updateRootObject();
            ++it;
        }
        else
        {
            it = m_properties.erase(it);
        }
    }
}

void QmlPropertyBase::ObjectHolder::addProperty(QmlPropertyBase* property)
{
    m_properties.append(property->m_state);
}

//-------------------------------------------------------------------------------------------------

QmlPropertyBase::QmlPropertyBase():
    m_state(new SharedState)
{
}

QmlPropertyBase::QmlPropertyBase(QObject* object, const QString& propertyName):
    QmlPropertyBase()
{
    m_state->setTarget(object, propertyName);
}

QmlPropertyBase::QmlPropertyBase(QQuickView* quickView, const QString& propertyName):
    QmlPropertyBase()
{
    QObject::connect(quickView, &QQuickView::statusChanged, m_state.get(),
        [quickView, propertyName, state = QPointer(m_state.get())]()
        {
            if (state)
                state->setTarget((QObject*) quickView->rootObject(), propertyName);
        });

    m_state->setTarget((QObject*) quickView->rootObject(), propertyName);
}

QmlPropertyBase::QmlPropertyBase(ObjectHolder* holder, const QString& propertyName):
    QmlPropertyBase()
{
    m_state->objectHolder = holder;
    m_state->setTarget(holder->object(), propertyName);
    holder->addProperty(this);
}

QmlPropertyBase::QmlPropertyBase(
    const QmlPropertyBase* objectProperty, const QString& propertyName)
    :
    QmlPropertyBase()
{
    const auto sourceState = QPointer(objectProperty->m_state.get());
    objectProperty->connectNotifySignal(
        m_state.get(),
        [sourceState, propertyName, state = QPointer(m_state.get())]()
        {
            if (!state || !sourceState)
                return;

            const QVariant value = sourceState->property.read();
            if (value.isNull())
            {
                state->setTarget(nullptr, propertyName);
                return;
            }

            auto object = value.value<QObject*>();
            NX_ASSERT(object, "The property value %1 is not a QObject.", propertyName);
            state->setTarget(object, propertyName);
        });

    m_state->setTarget(
        sourceState->property.read().value<QObject*>(), propertyName);
}

bool QmlPropertyBase::isValid() const
{
    return m_state->property.isValid();
}

bool QmlPropertyBase::hasNotifySignal() const
{
    return m_state->property.hasNotifySignal();
}

QMetaObject::Connection QmlPropertyBase::connectNotifySignal(
    QObject* receiver, const char* slot) const
{
    return QObject::connect(m_state.get(), SIGNAL(valueChanged()), receiver, slot);
}

QQmlProperty QmlPropertyBase::property() const
{
    return m_state->property;
}

detail::QmlPropertyConnection* QmlPropertyBase::connection() const
{
    return m_state.get();
}

template<>
QVariant QmlProperty<QVariant>::value() const
{
    return property().read();
}

template<>
bool QmlProperty<QVariant>::setValue(const QVariant& value) const
{
    return property().write(value);
}

} // namespace nx::vms::client::core
