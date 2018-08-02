#include "resource_node_view.h"

#include "resource_node_view_constants.h"
#include "resource_node_view_item_delegate.h"
#include "../details/node_view_store.h"
#include "../details/node_view_model.h"
#include "../details/node_view_state_patch.h"
#include "../details/node/view_node_helpers.h"

#include <QtWidgets/QHeaderView>

namespace {

using namespace nx::client::desktop::node_view;

static const details::ColumnsSet kSelectionColumns = { resourceCheckColumn };

} // namespace

namespace nx {
namespace client {
namespace desktop {
namespace node_view {

using namespace details;

struct ResourceNodeView::Private
{
    Private(QTreeView* view, const ColumnsSet& selectionColumns);

    ResourceNodeViewItemDelegate itemDelegate;
};

ResourceNodeView::Private::Private(QTreeView* view, const ColumnsSet& selectionColumns):
    itemDelegate(view, selectionColumns)
{
}

//-------------------------------------------------------------------------------------------------

ResourceNodeView::ResourceNodeView(QWidget* parent):
    base_type(resourceColumnsCount, kSelectionColumns, parent),
    d(new Private(this, kSelectionColumns))
{
    setItemDelegate(&d->itemDelegate);

    // We have to emit data changed signals for each checked state change to
    // correct repaint items in resource view.
    connect(&store(), &NodeViewStore::patchApplied, this,
        [this, &model = sourceModel()](const NodeViewStatePatch& patch)
        {
            for (const auto step: patch.steps)
            {
                if (step.operation != ChangeNodeOperation)
                    continue;

                if (!checkable(step.data, resourceCheckColumn))
                    continue;

                if (step.data.hasDataForColumn(resourceNameColumn))
                    continue;

                const auto index = model.index(step.path, resourceNameColumn);
                model.dataChanged(index, index, {Qt::ForegroundRole});
            }
        });
}

ResourceNodeView::~ResourceNodeView()
{
}

void ResourceNodeView::setupHeader()
{
    const auto treeHeader = header();
    treeHeader->setStretchLastSection(false);
    treeHeader->setSectionResizeMode(resourceNameColumn, QHeaderView::Stretch);
    treeHeader->setSectionResizeMode(resourceCheckColumn, QHeaderView::ResizeToContents);
}

} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx

