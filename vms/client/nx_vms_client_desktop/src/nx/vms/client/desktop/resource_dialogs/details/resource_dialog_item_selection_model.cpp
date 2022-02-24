// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_dialog_item_selection_model.h"

#include <nx/vms/client/desktop/common/models/item_model_algorithm.h>

namespace nx::vms::client::desktop {

ResourceDialogItemSelectionModel::ResourceDialogItemSelectionModel(
    QAbstractItemModel* model,
    QObject* parent)
    :
    base_type(model, parent)
{
}

void ResourceDialogItemSelectionModel::select(
    const QItemSelection& selection,
    QItemSelectionModel::SelectionFlags command)
{
    if (!(command.testFlag(QItemSelectionModel::Select)
            || command.testFlag(QItemSelectionModel::Deselect))
        || !model())
    {
        base_type::select(selection, command);
        return;
    }

    const auto selectionIndexes = selection.indexes();

    QSet<QModelIndex> extendedSelectionNodeIndexes;
    for (const auto& index: selectionIndexes)
    {
        static constexpr int kTreeColumnIndex = 0;
        if (model()->rowCount(index.siblingAtColumn(kTreeColumnIndex)) > 0)
        {
            extendedSelectionNodeIndexes.insert(index.siblingAtColumn(kTreeColumnIndex));
            const auto childNodeIndexes =
                item_model::getNonLeafIndexes(model(), index.siblingAtColumn(kTreeColumnIndex));
            for (const auto& childIndex: childNodeIndexes)
                extendedSelectionNodeIndexes.insert(childIndex);
        }
    }

    QItemSelection extendedSelection;
    extendedSelection.merge(selection, QItemSelectionModel::Select);

    for (const auto& nodeIndex: std::as_const(extendedSelectionNodeIndexes))
    {
        const auto rowCount = model()->rowCount(nodeIndex);
        const auto columnCount = model()->columnCount(nodeIndex);

        const auto topLeftIndex = model()->index(0, 0, nodeIndex);
        const auto bottomRightIndex = model()->index(rowCount - 1, columnCount - 1, nodeIndex);

        extendedSelection.merge(
            QItemSelection(topLeftIndex, bottomRightIndex), QItemSelectionModel::Select);
    }

    base_type::select(extendedSelection, command);
}

} // namespace nx::vms::client::desktop
