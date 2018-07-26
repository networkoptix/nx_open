#include "resource_node_view.h"

#include "resource_node_view_constants.h"

#include <QtWidgets/QHeaderView>

#include <nx/client/desktop/resource_views/node_view/node_view_constants.h>
#include <nx/client/desktop/resource_views/node_view/node_view_state_patch.h>
#include <nx/client/desktop/resource_views/node_view/node/view_node_helpers.h>
#include <nx/client/desktop/resource_views/node_view/resource_node_view/resource_node_view_item_delegate.h>
#include <nx/client/desktop/resource_views/node_view/details/node_view_store.h>
#include <nx/client/desktop/resource_views/node_view/details/node_view_model.h>

namespace {

using namespace nx::client::desktop;

static const ColumnsSet kSelectionColumns = { node_view::resourceCheckColumn };

} // namespace

namespace nx {
namespace client {
namespace desktop {

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
    base_type(node_view::resourceNodeViewColumnCount, kSelectionColumns, parent),
    d(new Private(this, kSelectionColumns))
{
    setItemDelegate(&d->itemDelegate);

    // We have to emit data changed signals for each checked state change to
    // correct repaint items in resource view.
    connect(&store(), &details::NodeViewStore::patchApplied, this,
        [this, &model = sourceModel()](const NodeViewStatePatch& patch)
        {
            for (const auto data: patch.changedData)
            {
                if (!node_view::helpers::isCheckable(data.data, node_view::resourceCheckColumn))
                    continue;

                if (data.data.hasDataForColumn(node_view::resourceNameColumn))
                    continue;

                const auto index = model.index(data.path, node_view::resourceNameColumn);
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
    treeHeader->setSectionResizeMode(node_view::resourceNameColumn, QHeaderView::Stretch);
    treeHeader->setSectionResizeMode(node_view::resourceCheckColumn, QHeaderView::ResizeToContents);
}

} // namespace desktop
} // namespace client
} // namespace nx

