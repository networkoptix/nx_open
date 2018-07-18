#include "resource_node_view.h"

#include <nx/client/desktop/resource_views/node_view/node_view_store.h>
#include <nx/client/desktop/resource_views/node_view/node_view_model.h>
#include <nx/client/desktop/resource_views/node_view/node_view_constants.h>
#include <nx/client/desktop/resource_views/node_view/node_view_state_patch.h>
#include <nx/client/desktop/resource_views/node_view/resource_node_view/resource_node_view_item_delegate.h>

namespace nx {
namespace client {
namespace desktop {

struct ResourceNodeView::Private
{
    Private(QTreeView* view);

    ResourceNodeViewItemDelegate itemDelegate;
};

ResourceNodeView::Private::Private(QTreeView* view):
    itemDelegate(view)
{
}

//-------------------------------------------------------------------------------------------------

ResourceNodeView::ResourceNodeView(QWidget* parent):
    base_type(parent),
    d(new Private(this))
{
    setItemDelegate(&d->itemDelegate);

    // We have to emit data changed signals for each checked state change to
    // correct repaint items in resource view.
    connect(store(), &NodeViewStore::patchApplied, this,
        [model = sourceModel()](const NodeViewStatePatch& patch)
        {
            for (const auto data: patch.changedData)
            {
                const auto itCheckColumnData = data.data.data.find(node_view::checkMarkColumn);
                if (itCheckColumnData == data.data.data.end())
                    continue;

                const auto hasCheckUpdate = itCheckColumnData.value().contains(Qt::CheckStateRole);
                if (!hasCheckUpdate)
                    continue;

                const auto itNameColumnData = data.data.data.find(node_view::nameColumn);
                if (itNameColumnData != data.data.data.end())
                    continue;

                const auto index = model->index(data.path, node_view::nameColumn);
                model->dataChanged(index, index, {Qt::ForegroundRole});
            }
        });
}

ResourceNodeView::~ResourceNodeView()
{
}

} // namespace desktop
} // namespace client
} // namespace nx

