// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QItemSelection>

namespace nx::vms::client::desktop {

class ItemModelUtils: public QObject
{
    Q_OBJECT

public:
    Q_INVOKABLE static QItemSelection createSelection(
        const QModelIndex& topLeft, const QModelIndex& bottomRight);

    static void registerQmlType();
};

} // namespace nx::vms::client::desktop
