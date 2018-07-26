#include "selection_node_view.h"

#include "selection_node_view_state_reducer.h"

#include <nx/client/desktop/resource_views/node_view/node_view_state.h>
#include <nx/client/desktop/resource_views/node_view/node/view_node_helpers.h>
#include <nx/client/desktop/resource_views/node_view/details/node_view_model.h>
#include <nx/client/desktop/resource_views/node_view/details/node_view_store.h>

#include <nx/client/desktop/common/utils/item_view_utils.h>

namespace nx {
namespace client {
namespace desktop {
namespace node_view {

struct SelectionNodeView::Private: public QObject
{
    Private(
        SelectionNodeView* owner,
        const ColumnsSet& selectionColumns);

    void handleDataChange(const QModelIndex& index, const QVariant& value, int role);

    SelectionNodeView* const owner;
    ColumnsSet selectionColumns;
    ItemViewUtils::CheckableCheckFunction checkableCheck;
};

SelectionNodeView::Private::Private(
    SelectionNodeView* owner,
    const ColumnsSet& selectionColumns)
    :
    owner(owner),
    selectionColumns(selectionColumns),
    checkableCheck([owner](const QModelIndex& index) { return helpers::isCheckable(index); })
{
}

void SelectionNodeView::Private::handleDataChange(
    const QModelIndex& index,
    const QVariant& value,
    int role)
{
    if (role != Qt::CheckStateRole || !selectionColumns.contains(index.column()))
        return;

    const auto node = helpers::nodeFromIndex(index);
    owner->applyPatch(SelectionNodeViewStateReducer::setNodeSelected(
        owner->state(), selectionColumns, node->path(), index.column(), value.value<Qt::CheckState>()));
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

    connect(&sourceModel(), &details::NodeViewModel::dataChangeOccured,
        d, &Private::handleDataChange);
}

SelectionNodeView::~SelectionNodeView()
{
}

} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx

