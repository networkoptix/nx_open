#include "field.h"

#include <QDebug>
#include <QEvent>
#include <QJsonValue>
#include <QMetaProperty>
#include <QScopedValueRollback>

namespace nx::vms::rules {

const QString kMetatype = "metatype";

Field::Field()
{
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
    for (int i = meta->propertyOffset(); i < meta->propertyCount(); ++i)
    {
        const auto& propName = meta->property(i).name();
        serialized.insert(propName, this->property(propName).toJsonValue());
    }

    for (const auto& propName: this->dynamicPropertyNames())
    {
        serialized.insert(propName, this->property(propName).toJsonValue());
    }

    return serialized;
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
