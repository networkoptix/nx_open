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

} // namespace nx::utils
