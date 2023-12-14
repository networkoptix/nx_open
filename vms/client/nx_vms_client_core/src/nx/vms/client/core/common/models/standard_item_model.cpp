// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "standard_item_model.h"

#include <QtCore/QSignalBlocker>
#include <QtCore/private/qabstractitemmodel_p.h>

namespace nx::vms::client::core {

bool StandardItemModel::moveRows(const QModelIndex& sourceParent, int sourceRow, int count,
    const QModelIndex& destinationParent, int destinationChild)
{
    const bool validArgs = beginMoveRows(sourceParent, sourceRow, sourceRow + count - 1,
        destinationParent, destinationChild);

    if (!validArgs)
        return false;

    auto sourceItem = itemFromIndex(sourceParent);
    auto destinationItem = itemFromIndex(destinationParent);

    if (sourceParent == destinationParent && sourceRow < destinationChild)
        destinationChild -= count;

    // Preserve persistent indices: they mustn't suffer from row removal and re-insertion.
    Q_D(QAbstractItemModel);
    const auto savedPersistentIndexes = d->persistent.indexes;
    d->persistent.indexes.clear();

    QSignalBlocker blocker(this);

    QList<QList<QStandardItem*>> rows;
    for (int i = 0; i < count; ++i)
        rows << (sourceItem ? sourceItem->takeRow(sourceRow) : takeRow(sourceRow));

    for (const auto& row: rows)
    {
        if (destinationItem)
            destinationItem->insertRow(destinationChild++, row);
        else
            insertRow(destinationChild++, row);
    }

    blocker.unblock();

    // Restore persistent indices, they will be properly updated in endMoveRows().
    d->persistent.indexes = savedPersistentIndexes;

    endMoveRows();

    return true;
}

} // namespace nx::vms::client::core
