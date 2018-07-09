#include "multiple_layout_selection_dialog.h"
#include "ui_multiple_layout_selection_dialog.h"

#include <nx/client/desktop/resource_views/node_view/nodes/view_node.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node_helpers.h>
#include <nx/client/desktop/resource_views/node_view/node_view_state.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node_helpers.h>
#include <nx/client/desktop/resource_views/layout/accessible_layout_sort_model.h>

//namespace {

//using namespace nx::client::desktop;

//static const auto createLayoutParentNode =
//    [](const QnResourcePtr& resource) -> NodePtr
//    {
//        return ResourceNode::create(resource);
//        //
//        const auto layout = resource.dynamicCast<QnLayoutResource>();
//        if (!layout)
//            return NodePtr();

//        QSet<QnUuid> resourceIds;
//        for (const auto& itemData: layout->getItems())
//            resourceIds.insert(itemData.resource.id);

//        const auto relationCheckFunction =
//            [resourceIds](const QnResourcePtr& /*parent*/, const QnResourcePtr& child)
//            {
//                return resourceIds.contains(child->getId());
//            };

//        return ParentResourceNode::create(resource, relationCheckFunction);
//    };

//} // namespace

namespace nx {
namespace client {
namespace desktop {

bool MultipleLayoutSelectionDialog::getLayouts(QWidget* parent, QnResourceList& resources)
{
    MultipleLayoutSelectionDialog dialog(parent);

    if (dialog.exec() != QDialog::Accepted)
        return false;

    resources = helpers::getLeafSelectedResources(dialog.ui->layoutsTree->state().rootNode);
    return true;
}

MultipleLayoutSelectionDialog::MultipleLayoutSelectionDialog(QWidget* parent):
    base_type(parent),
    ui(new Ui::MultipleLayoutSelectionDialog)
{
    ui->setupUi(this);

    const auto tree = ui->layoutsTree;
    tree->setState({helpers::createParentedLayoutsNode()});
    //tree->setState({helpers::createCurrentUserLayoutsNode()});
    tree->setExpandsOnDoubleClick(true);
    tree->expandAll();

    tree->setProxyModel(new AccessibleLayoutSortModel(this));
}

MultipleLayoutSelectionDialog::~MultipleLayoutSelectionDialog()
{
}

} // namespace desktop
} // namespace client
} // namespace nx
