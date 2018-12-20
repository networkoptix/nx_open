#include "resource_selection_node_view.h"

#include "resource_view_node_helpers.h"
#include "resource_node_view_constants.h"
#include "details/resource_node_view_item_selection_delegate.h"
#include "../selection_node_view/selection_node_view_constants.h"
#include "../details/node/view_node.h"
#include "../details/node_view_store.h"
#include "../details/node_view_model.h"
#include "../details/node_view_state_patch.h"
#include "../details/node/view_node_helpers.h"

#include <nx/utils/uuid.h>
#include <core/resource/resource.h>

#include <QtWidgets/QHeaderView>

namespace {

using namespace nx::vms::client::desktop::node_view;

static const details::ColumnSet kSelectionColumns = { resourceCheckColumn };

} // namespace

namespace nx::vms::client::desktop {
namespace node_view {

using namespace details;

struct ResourceSelectionNodeView::Private: public QObject
{
    Private(ResourceSelectionNodeView* owner, const ColumnSet& selectionColumns);
    void handlePatchApplied(const NodeViewStatePatch& patch);

    ResourceSelectionNodeView* q;
    ResourceNodeViewItemSelectionDelegate itemDelegate;
    ResourceSelectionNodeView::SelectionMode mode = ResourceSelectionNodeView::simpleSelectionMode;
};

ResourceSelectionNodeView::Private::Private(
    ResourceSelectionNodeView* owner,
    const ColumnSet& selectionColumns)
    :
    q(owner),
    itemDelegate(owner, selectionColumns)
{
}

void ResourceSelectionNodeView::Private::handlePatchApplied(const NodeViewStatePatch& patch)
{
    auto& model = q->sourceModel();
    for (const auto step: patch.steps)
    {
        if (step.operation != ChangeNodeOperation)
            continue;

        const auto hasOtherChange = step.data.hasDataForColumn(resourceNameColumn);
        if (hasOtherChange)
            continue;

        const bool hasCheckChange = checkable(step.data, resourceCheckColumn)
            && step.data.hasDataForColumn(resourceCheckColumn);
        if (!hasCheckChange && !step.data.hasProperty(selectedChildrenCountProperty))
            continue;

        const auto index = model.index(step.path, resourceNameColumn);
        model.dataChanged(index, index, {Qt::ForegroundRole});
    }
}

//-------------------------------------------------------------------------------------------------

ResourceSelectionNodeView::ResourceSelectionNodeView(QWidget* parent):
    base_type(resourceColumnsCount, kSelectionColumns, parent),
    d(new Private(this, kSelectionColumns))
{
    setItemDelegate(&d->itemDelegate);

    // We have to emit data changed signals for each checked state change to
    // correct repaint items in resource view.
    connect(&store(), &NodeViewStore::patchApplied, d, &Private::handlePatchApplied);

    connect(this, &SelectionNodeView::selectionChanged, this,
        [this](const ViewNodePath& path, Qt::CheckState checkedState)
        {
            const auto node = state().rootNode->nodeAt(path);
            auto resource = getResource(node);
            if (resource)
                emit resourceSelectionChanged(resource->getId(), checkedState);
        });
}

ResourceSelectionNodeView::~ResourceSelectionNodeView()
{
}

void ResourceSelectionNodeView::setupHeader()
{
    const auto treeHeader = header();
    treeHeader->setStretchLastSection(false);
    treeHeader->setSectionResizeMode(resourceNameColumn, QHeaderView::Stretch);
    treeHeader->setSectionResizeMode(resourceCheckColumn, QHeaderView::ResizeToContents);
    treeHeader->hide();

    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

void ResourceSelectionNodeView::setSelectionMode(SelectionMode mode)
{
    d->mode = mode;
}

void ResourceSelectionNodeView::setLeafResourcesSelected(const QnUuidSet& resourceIds, bool select)
{
    PathList paths;
    details::forEachLeaf(state().rootNode,
        [resourceIds, &paths](const details::NodePtr& node)
        {
            const auto resource = getResource(node);
            if (resource && resourceIds.contains(resource->getId()))
                paths.append(node->path());
        });

    setSelectedNodes(paths, select);
}

void ResourceSelectionNodeView::handleDataChangeRequest(
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
} // namespace nx::vms::client::desktop

