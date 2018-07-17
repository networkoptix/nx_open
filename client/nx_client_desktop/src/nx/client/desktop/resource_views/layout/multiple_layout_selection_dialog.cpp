#include "multiple_layout_selection_dialog.h"
#include "ui_multiple_layout_selection_dialog.h"

#include <QtWidgets/QItemDelegate>

#include <nx/client/desktop/resource_views/node_view/node_view_model.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node.h>
#include <nx/client/desktop/resource_views/node_view/node_view_constants.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node_helpers.h>
#include <nx/client/desktop/resource_views/node_view/node_view_state.h>
#include <nx/client/desktop/resource_views/node_view/node_view_state_reducer.h>
#include <nx/client/desktop/resource_views/node_view/sort/node_view_group_sorting_model.h>
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

#include <QtGui/QPainter>

namespace {

using namespace nx::client::desktop;

class Delegate: public QItemDelegate
{
    using base_type = QItemDelegate;

public:
    Delegate(QTreeView* owner = nullptr);

    virtual void paint(
        QPainter *painter,
        const QStyleOptionViewItem &option,
        const QModelIndex &index) const override;

private:
    static QModelIndex getMappedIndex(const QModelIndex& index, QAbstractItemModel* model);

private:
    QTreeView const * m_owner;
};

Delegate::Delegate(QTreeView* owner):
    base_type(owner),
    m_owner(owner)
{

}

void Delegate::paint(
    QPainter *painter,
    const QStyleOptionViewItem &option,
    const QModelIndex &index) const
{
    base_type::paint(painter, option, index);

    const auto node = NodeViewModel::nodeFromIndex(getMappedIndex(index, m_owner->model()));
    if (!node || !node->data(node_view::nameColumn, node_view::separatorRole).toBool())
        return;

    painter->setBrush(Qt::red);
    painter->fillRect(option.rect, Qt::red);
}

QModelIndex Delegate::getMappedIndex(const QModelIndex& index, QAbstractItemModel* model)
{
    const auto proxy = qobject_cast<QSortFilterProxyModel*>(model);
    return proxy
        ? getMappedIndex(proxy->mapToSource(index), proxy->sourceModel())
        : index;
}


} // namespace

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
    const auto siblingGroupProxyModel = new NodeViewGroupSortingModel(this);
    siblingGroupProxyModel->setSourceModel(proxyModel);

    const auto tree = ui->layoutsTree;
    tree->setItemDelegate(new Delegate(tree));
    tree->setProxyModel(siblingGroupProxyModel);
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

    tree->setExpandsOnDoubleClick(true);
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
