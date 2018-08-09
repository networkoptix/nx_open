#include "selection_node_view.h"

#include "selection_node_view_state_reducer.h"
#include "../details/node_view_model.h"
#include "../details/node_view_state.h"
#include "../details/node_view_store.h"
#include "../details/node/view_node.h"
#include "../details/node/view_node_helpers.h"
#include "../node_view/node_view_state_reducer.h"

#include <nx/client/desktop/common/utils/item_view_utils.h>

namespace nx {
namespace client {
namespace desktop {
namespace node_view {

using namespace details;

struct SelectionNodeView::Private: public QObject
{
    Private(
        SelectionNodeView* owner,
        const ColumnsSet& selectionColumns);

    SelectionNodeView* const owner;
    ColumnsSet selectionColumns;
    ItemViewUtils::IsCheckableFunction checkableCheck;
};

SelectionNodeView::Private::Private(
    SelectionNodeView* owner,
    const ColumnsSet& selectionColumns)
    :
    owner(owner),
    selectionColumns(selectionColumns),
    checkableCheck([owner](const QModelIndex& index) { return checkable(index); })
{
}

//-------------------------------------------------------------------------------------------------

SelectionNodeView::SelectionNodeView(
    int columnsCount,
    const ColumnsSet& selectionColumns,
    QWidget* parent)
    :
    base_type(columnsCount, parent),
    d(new Private(this, selectionColumns))
{
    for (const int column: selectionColumns)
        ItemViewUtils::setupDefaultAutoToggle(this, column, d->checkableCheck);
}

SelectionNodeView::~SelectionNodeView()
{
}

void SelectionNodeView::setSelectedNodes(const PathList& paths, bool value)
{
    if (d->selectionColumns.isEmpty())
        return;

    NodeViewStatePatch patch;
    const auto& state = store().state();
    for (const auto path: paths)
    {
        const auto selectionPatch = SelectionNodeViewStateReducer::setNodeSelected(
            state, d->selectionColumns, path, value ? Qt::Checked : Qt::Unchecked);
        patch.appendPatchSteps(selectionPatch);
    }

    applyPatch(patch);
}

void SelectionNodeView::handleSourceModelDataChanged(
    const QModelIndex& index,
    const QVariant& value,
    int role)
{
    if (role != Qt::CheckStateRole //< Base implementation possibly can handle other role.
        || !checkable(index)
        || !d->selectionColumns.contains(index.column()))
    {
        base_type::handleSourceModelDataChanged(index, value, role);
        return;
    }

    const auto node = nodeFromIndex(index);
    const auto checkedState = value.value<Qt::CheckState>();
    applyPatch(SelectionNodeViewStateReducer::setNodeSelected(
        state(), d->selectionColumns, node->path(), checkedState));
}

} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx

