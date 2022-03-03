// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "field.h"

#include <QDebug>
#include <QEvent>
#include <QJsonValue>
#include <QMetaProperty>
#include <QScopedValueRollback>

#include <nx/fusion/serialization/json.h>

namespace nx::vms::rules {

Field::Field()
{
}

QString Field::metatype() const
{
    int idx = metaObject()->indexOfClassInfo(kMetatype);
    // NX_ASSERT(idx >= 0, "Class does not have required 'metatype' property");

    return metaObject()->classInfo(idx).value();
}

void Field::connectSignals()
{
    if (m_connected)
        return;

    auto meta = metaObject();
    for (int i = meta->propertyOffset(); i < meta->propertyCount(); ++i)
    {
        const auto& prop = meta->property(i);
        if (!prop.hasNotifySignal())
        {
            qDebug() << "Property" << prop.name()
                << "of an" << meta->className() << "instance has no notify signal";
                //< TODO: #spanasenko Improve diagnostics.
        }

        connect(this, prop.notifySignal().methodSignature().data(), this, SLOT(notifyParent()));
    }

    m_connected = true;
}

QMap<QString, QJsonValue> Field::serializedProperties() const
{
    QMap<QString, QJsonValue> serialized;
    auto meta = metaObject();

    for (int i = base_type::staticMetaObject.propertyCount(); i < meta->propertyCount(); ++i)
    {
        auto prop = meta->property(i);
        auto userType = prop.userType();

        if (!NX_ASSERT(
            userType != QMetaType::UnknownType,
            "Unregistered prop type: %1",
            prop.typeName()))
        {
            continue;
        }

        auto serializer = QnJsonSerializer::serializer(prop.userType());
        if (!NX_ASSERT(serializer, "Unregistered serializer for prop type: %1", prop.typeName()))
            continue;

        QJsonValue jsonValue;
        QnJsonContext ctx;

        serializer->serialize(&ctx, prop.read(this), &jsonValue);

        if (NX_ASSERT(!jsonValue.isUndefined()))
            serialized.insert(prop.name(), std::move(jsonValue));
    }

    for (const auto& propName: this->dynamicPropertyNames())
    {
        serialized.insert(propName, this->property(propName).toJsonValue());
    }

    return serialized;
}

bool Field::setProperties(const QVariantMap& properties)
{
    bool isAllPropertiesSet = true;
    std::for_each(properties.constKeyValueBegin(), properties.constKeyValueEnd(),
        [this, &isAllPropertiesSet](const std::pair<QString, QVariant>& p){
            if (!setProperty(p.first.toUtf8(), p.second))
            {
                NX_ERROR(this, "Failed to set property %1 for the %2 field", p.first, metatype());
                isAllPropertiesSet = false;
            }
        });

    return isAllPropertiesSet;
}

bool Field::event(QEvent* ev)
{
    if (m_connected && ev->type() == QEvent::DynamicPropertyChange)
        notifyParent();

    return QObject::event(ev);
}

void Field::notifyParent()
{
    if (m_updateInProgress)
        return;

    QScopedValueRollback<bool> guard(m_updateInProgress, true);
    emit stateChanged();
}

} // namespace nx::vms::rules
