// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "item_model_algorithm.h"

namespace nx::vms::client::desktop {
namespace item_model {

QModelIndexList getNonLeafIndexes(const QAbstractItemModel* model, const QModelIndex& parent)
{
    QModelIndexList result;
    for (int row = 0; row < model->rowCount(parent); ++row)
    {
        const auto childIndex = model->index(row, 0, parent);
        const auto childFlags = childIndex.flags();

        if (childFlags.testFlag(Qt::ItemNeverHasChildren) || model->rowCount(childIndex) == 0)
            continue;

        result.append(childIndex);
        result.append(getNonLeafIndexes(model, childIndex));
    }
    return result;
}

QModelIndexList getLeafIndexes(const QAbstractItemModel* model, const QModelIndex& parent)
{
    QModelIndexList result;
    for (int row = 0; row < model->rowCount(parent); ++row)
    {
        const auto childIndex = model->index(row, 0, parent);
        const auto childFlags = childIndex.flags();

        if (childFlags.testFlag(Qt::ItemNeverHasChildren) || model->rowCount(childIndex) == 0)
            result.append(childIndex);
        else
            result.append(getLeafIndexes(model, childIndex));
    }
    return result;
}

QModelIndexList getAllIndexes(const QAbstractItemModel* model, const QModelIndex& parent)
{
    QModelIndexList result;
    for (int row = 0; row < model->rowCount(parent); ++row)
    {
        const auto childIndex = model->index(row, 0, parent);
        result.append(childIndex);
        result.append(getAllIndexes(model, childIndex));
    }
    return result;
}

} // namespace item_model
} // namespace nx::vms::client::desktop
