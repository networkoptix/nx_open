#include "selection_node_view.h"

#include "selection_node_view_state_reducer.h"
#include "../details/node_view_model.h"
#include "../details/node_view_state.h"
#include "../details/node_view_store.h"
#include "../details/node/view_node.h"
#include "../details/node/view_node_helpers.h"
#include "../node_view/node_view_state_reducer.h"

#include <nx/vms/client/desktop/common/utils/item_view_utils.h>

namespace nx::vms::client::desktop {
namespace node_view {

using namespace details;

struct SelectionNodeView::Private: public QObject
{
    Private(
        SelectionNodeView* owner,
        const ColumnSet& selectionColumns);

    SelectionNodeView* const owner;
    ColumnSet selectionColumns;
    item_view_utils::IsCheckable checkableCheck;
};

SelectionNodeView::Private::Private(
    SelectionNodeView* q,
    const ColumnSet& selectionColumns)
    :
    owner(q),
    selectionColumns(selectionColumns),
    checkableCheck([q](const QModelIndex& index) { return checkable(index); })
{
}

//-------------------------------------------------------------------------------------------------

SelectionNodeView::SelectionNodeView(
    int columnsCount,
    const ColumnSet& selectionColumns,
    QWidget* parent)
    :
    base_type(columnsCount, parent),
    d(new Private(this, selectionColumns))
{
    for (const int column: selectionColumns)
        item_view_utils::setupDefaultAutoToggle(this, column, d->checkableCheck);

    connect(&sourceModel(), &NodeViewModel::dataChanged, this,
        [this](const QModelIndex &topLeft,
            const QModelIndex &bottomRight,
            const QVector<int> &roles)
        {
            NX_ASSERT(topLeft == bottomRight,
                "We expect that node view model updates data row by row");

            if (!roles.contains(Qt::CheckStateRole))
                return;

            if (!d->selectionColumns.contains(topLeft.column()))
                return;

            const auto node = nodeFromIndex(topLeft);
            if (!node)
                return;

            // Checks if we have emitted signal already. Just check if all other columns have
            // another value.
            const auto currentCheckedState = checkedState(topLeft);
            const bool hasEmittedSignal =
                std::any_of(d->selectionColumns.begin(), d->selectionColumns.end(),
                    [node, currentCheckedState, baseColumn = topLeft.column()](int column)
                    {
                        return column != baseColumn
                            && currentCheckedState == checkedState(node, column);
                    });
            emit selectionChanged(node->path(), currentCheckedState);
        });
}

SelectionNodeView::~SelectionNodeView()
{
}

void SelectionNodeView::setSelectedNodes(const PathList& paths, bool value)
{
    if (d->selectionColumns.isEmpty())
        return;

    const auto& state = store().state();
    for (const auto path: paths)
    {
        const auto selectionPatch = SelectionNodeViewStateReducer::setNodeSelected(
            state, d->selectionColumns, path, value ? Qt::Checked : Qt::Unchecked);
        applyPatch(selectionPatch);
    }
}

const details::ColumnSet& SelectionNodeView::selectionColumns() const
{
    return d->selectionColumns;
}

void SelectionNodeView::handleDataChangeRequest(
    const QModelIndex& index,
    const QVariant& value,
    int role)
{
    if (role != Qt::CheckStateRole //< Base implementation possibly can handle other role.
        || !checkable(index)
        || !d->selectionColumns.contains(index.column()))
    {
        base_type::handleDataChangeRequest(index, value, role);
        return;
    }

    const auto node = nodeFromIndex(index);
    const auto checkedState = value.value<Qt::CheckState>();
    applyPatch(SelectionNodeViewStateReducer::setNodeSelected(
        state(), d->selectionColumns, node->path(), checkedState));
}

} // namespace node_view
} // namespace nx::vms::client::desktop

