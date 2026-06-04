// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "qobject.h"

namespace nx::utils {

void watchOnPropertyChanges(
    QObject* sender,
    QObject* receiver,
    const QMetaMethod& receiverMetaMethod)
{
    const auto metaObject = sender->metaObject();
    const auto propertyNames = nx::utils::propertyNames(
        sender, PropertyAccess::anyAccess, /*includeBaseProperties*/ false, /*hasNotifySignal*/ true);
    for (const auto& propertyName: propertyNames)
    {
        const auto metaProperty =
            metaObject->property(metaObject->indexOfProperty(propertyName));

        QObject::connect(sender, metaProperty.notifySignal(), receiver, receiverMetaMethod);
    }
}

void NX_UTILS_API resetProperties(QObject* object)
{
    const auto meta = object->metaObject();
    static const auto start =
        QObject::staticMetaObject.propertyOffset() + QObject::staticMetaObject.propertyCount();

    for (int i = start; i < meta->propertyCount(); ++i)
    {
        const auto property = meta->property(i);
        if (property.isResettable())
            property.reset(object);
    }
}

} // namespace nx::utils
