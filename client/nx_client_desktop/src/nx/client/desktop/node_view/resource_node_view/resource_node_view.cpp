#include "resource_node_view.h"

#include "resource_view_node_helpers.h"
#include "resource_node_view_constants.h"
#include "resource_node_view_item_delegate.h"
#include "../details/node/view_node.h"
#include "../details/node_view_store.h"
#include "../details/node_view_model.h"
#include "../details/node_view_state_patch.h"
#include "../details/node/view_node_helpers.h"

#include <nx/utils/uuid.h>
#include <core/resource/resource.h>

#include <QtWidgets/QHeaderView>

namespace {

using namespace nx::client::desktop::node_view;

static const details::ColumnSet kSelectionColumns = { resourceCheckColumn };

} // namespace

namespace nx {
namespace client {
namespace desktop {
namespace node_view {

using namespace details;

struct ResourceNodeView::Private
{
    Private(QTreeView* view, const ColumnSet& selectionColumns);

    ResourceNodeViewItemDelegate itemDelegate;
    ResourceNodeView::SelectionMode mode = ResourceNodeView::simpleSelectionMode;
};

ResourceNodeView::Private::Private(QTreeView* view, const ColumnSet& selectionColumns):
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

    connect(this, &SelectionNodeView::selectionChanged, this,
        [this](const ViewNodePath& path, Qt::CheckState checkedState)
        {
            NX_EXPECT(checkedState != Qt::PartiallyChecked, "Resource can't be partially checked!");
            const auto node = state().rootNode->nodeAt(path);
            emit resourceSelectionChanged(getResource(node)->getId(), checkedState);
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
    treeHeader->hide();

    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

void ResourceNodeView::setSelectionMode(SelectionMode mode)
{
    d->mode = mode;
}

void ResourceNodeView::setLeafResourcesSelected(const UuidSet& resourceIds, bool select)
{
    PathList paths;
    details::forEachLeaf(store().state().rootNode,
        [resourceIds, &paths](const details::NodePtr& node)
        {
            const auto resource = getResource(node);
            if (resource && resourceIds.contains(resource->getId()))
                paths.append(node->path());
        });

    setSelectedNodes(paths, select);
}

void ResourceNodeView::handleDataChangeRequest(
    const QModelIndex& index,
    const QVariant& value,
    int role)
{
    if (d->mode == simpleSelectionMode || role != Qt::CheckStateRole
        || !selectionColumns().contains(index.column()))
    {
        base_type::handleDataChangeRequest(index, value, role);
    }
    else if (const auto resource = getResource(index))
    {
        setLeafResourcesSelected({resource->getId()},value.value<Qt::CheckState>() == Qt::Checked);
    }
}

} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx

