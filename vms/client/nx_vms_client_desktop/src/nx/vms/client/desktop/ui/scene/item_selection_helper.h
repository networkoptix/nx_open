#pragma once

#include <QtCore/QObject>
#include <QtCore/QItemSelection>

namespace nx::vms::client::desktop {

class ItemSelectionHelper: public QObject
{
    Q_OBJECT

public:
    Q_INVOKABLE QItemSelection createSelection(
        const QModelIndex& topLeft, const QModelIndex& bottomRight) const;

    static void registerQmlType();
};

} // namespace nx::vms::client::desktop
