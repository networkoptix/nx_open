#include "field.h"

#include <QDebug>
#include <QEvent>
#include <QMetaProperty>
#include <QScopedValueRollback>

namespace nx::vms::rules {

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
