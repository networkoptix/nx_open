// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "field.h"

#include <QtCore/QEvent>
#include <QtCore/QMetaProperty>
#include <QtCore/QScopedValueRollback>

#include <nx/utils/log/log.h>
#include <nx/utils/qobject.h>

#include "utils/serialization.h"

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
            // TODO: #spanasenko Improve diagnostics.
            NX_DEBUG(
                this,
                "Property %1 of an %2 instance has no notify signal",
                prop.name(),
                meta->className());
        }

        connect(this, prop.notifySignal().methodSignature().data(), this, SLOT(notifyParent()));
    }

    m_connected = true;
}

QMap<QString, QJsonValue> Field::serializedProperties() const
{
    return serializeProperties(this, nx::utils::propertyNames(this));
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
