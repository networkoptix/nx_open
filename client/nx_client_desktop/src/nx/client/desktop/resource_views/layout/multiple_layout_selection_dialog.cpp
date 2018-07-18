#include "multiple_layout_selection_dialog.h"
#include "ui_multiple_layout_selection_dialog.h"

#include <nx/client/desktop/resource_views/node_view/node_view_model.h>
#include <nx/client/desktop/resource_views/node_view/node_view_state.h>
#include <nx/client/desktop/resource_views/node_view/node_view_state_patch.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node_helpers.h>

#include <nx/client/desktop/resource_views/layout/accessible_layout_sort_model.h>

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
    using namespace helpers; //< TODO: remove me

    ui->setupUi(this);

    const auto proxyModel = new AccessibleLayoutSortModel(this);
    const auto tree = ui->layoutsTree;
    tree->setProxyModel(proxyModel);
    tree->applyPatch(NodeViewStatePatch::fromRootNode(helpers::createParentedLayoutsNode()));
//    tree->applyPatch(NodeViewStatePatch::fromRootNode(helpers::createCurrentUserLayoutsNode()));

//    tree->applyPatch(NodeViewStatePatch::fromRootNode(ViewNode::create({
//        createCheckAllNode(lit("Check All"), QIcon(), -2),
//        createSeparatorNode(-1),
//        createNode(lit("1"), {
//            createCheckAllNode(lit("Check All #1"), QIcon(), -2),
//            createSeparatorNode(-1),
//            createNode(lit("1_1")),
//            createNode(lit("1_2")),
//            createSeparatorNode(1),
//            createCheckAllNode(lit("Check All #1"), QIcon(), 2),
//            }),
//        createNode(lit("2")),
//        createNode(lit("3"), 2),
//    })));

//    tree->applyPatch(NodeViewStatePatch::fromRootNode(
//        ViewNode::create({helpers::createSeparatorNode()})));

    tree->expandAll();
    connect(ui->searchLineEdit, &SearchLineEdit::textChanged, this,
        [proxyModel](const QString& text) { proxyModel->setFilter(text); });
}

MultipleLayoutSelectionDialog::~MultipleLayoutSelectionDialog()
{
}

} // namespace desktop
} // namespace client
} // namespace nx
