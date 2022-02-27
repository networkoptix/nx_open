// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "item_model_utils.h"

#include <QtQml/QtQml>

#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {

QItemSelection ItemModelUtils::createSelection(
    const QModelIndex& topLeft, const QModelIndex& bottomRight)
{
    return QItemSelection(topLeft, bottomRight);
}

void ItemModelUtils::registerQmlType()
{
    qmlRegisterSingletonType<ItemModelUtils>("nx.vms.client.desktop", 1, 0, "ItemModelUtils",
        [](QQmlEngine*, QJSEngine*) -> QObject* { return new ItemModelUtils(); });
};

} // namespace nx::vms::client::desktop
