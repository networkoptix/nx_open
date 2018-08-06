#include "multiple_layout_selection_dialog.h"
#include "ui_multiple_layout_selection_dialog.h"

#include <nx/client/desktop/node_view/details/node_view_store.h>
#include <nx/client/desktop/node_view/details/node_view_state_patch.h>
#include <nx/client/desktop/node_view/details/node/view_node.h>
#include <nx/client/desktop/node_view/details/node/view_node_helpers.h>
#include <nx/client/desktop/node_view/node_view/sorting/node_view_base_sort_model.h>
#include <nx/client/desktop/node_view/selection_node_view/selection_view_node_helpers.h>
#include <nx/client/desktop/node_view/resource_node_view/resource_node_view_constants.h>
#include <nx/client/desktop/node_view/resource_node_view/resource_view_node_helpers.h>
#include <nx/client/desktop/node_view/resource_node_view/resource_node_view_state_reducer.h>

#include <common/common_module.h>
#include <client_core/client_core_module.h>
#include <core/resource/user_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_access/providers/resource_access_provider.h>
#include <nx/client/core/watchers/user_watcher.h>
#include <ui/widgets/common/snapped_scrollbar.h>

namespace {

using namespace nx::client::desktop::node_view;
using namespace nx::client::desktop::node_view::details;

QnUuid getCurrentUserId()
{
    const auto commonModule = qnClientCoreModule->commonModule();
    const auto userWatcher = commonModule->instance<nx::client::core::UserWatcher>();
    const auto currentUser = userWatcher->user();
    return currentUser->getId();
}

//-------------------------------------------------------------------------------------------------

class AccessibleLayoutSortModel: public NodeViewBaseSortModel
{
    using base_type = NodeViewBaseSortModel;

public:
    AccessibleLayoutSortModel(int column, QObject* parent = nullptr);

    // TODO: move to base proxy of node view
    void setFilter(const QString& filter);

protected:
    virtual bool lessThan(
        const QModelIndex& sourceLeft,
        const QModelIndex& sourceRight) const override;

    virtual bool filterAcceptsRow(
        int sourceRow,
        const QModelIndex &sourceParent) const override;

private:
    const int m_column;
    QString m_filter;
};

AccessibleLayoutSortModel::AccessibleLayoutSortModel(int column, QObject* parent):
    m_column(column)
{
}

void AccessibleLayoutSortModel::setFilter(const QString& filter)
{
    m_filter = filter;
    invalidateFilter();
}

bool AccessibleLayoutSortModel::lessThan(
    const QModelIndex& sourceLeft,
    const QModelIndex& sourceRight) const
{
    const auto leftResource = getResource(sourceLeft);
    const auto rightResource = getResource(sourceRight);
    if (!leftResource || !rightResource)
        return !base_type::lessThan(sourceLeft, sourceRight);

    const auto source = sourceModel();
    const bool isGroup = source->rowCount(sourceLeft) > 0 || source->rowCount(sourceRight) > 0;
    if (isGroup)
    {
        const auto currentUserId = getCurrentUserId();
        if (leftResource && leftResource->getId() == currentUserId)
            return false;

        if (rightResource && rightResource->getId() == currentUserId)
            return true;
    }

    return !base_type::lessThan(sourceLeft, sourceRight);
}

bool AccessibleLayoutSortModel::filterAcceptsRow(
    int sourceRow,
    const QModelIndex &sourceParent) const
{
    if (m_filter.isEmpty())
        return true;

    const auto source = sourceModel();
    if (!source)
        return true;

    const auto index = source->index(sourceRow, m_column, sourceParent);
    const auto text = index.data().toString();
    const bool containsFilter = text.contains(m_filter);
    if (containsFilter)
        return true;

    const auto childrenCount = source->rowCount(index);
    for (int i = 0; i != childrenCount; ++i)
    {
        if (filterAcceptsRow(i, index))
            return true;
    }
    return false;
}

//-------------------------------------------------------------------------------------------------

const auto createCheckableLayoutNode =
    [](const QnResourcePtr& resource) -> NodePtr
    {
        return createResourceNode(resource, Qt::Unchecked);
    };

using UserResourceList = QList<QnUserResourcePtr>;
NodePtr createUserLayoutsNode(
    const UserResourceList& users,
    const ChildrenCountExtraTextGenerator& childrenCountTextGenerator)
{
    const auto commonModule = qnClientCoreModule->commonModule();
    const auto accessProvider = commonModule->resourceAccessProvider();
    const auto userWatcher = commonModule->instance<nx::client::core::UserWatcher>();
    const auto currentUserId = userWatcher->user()->getId();

    QSet<QnUuid> accessibleUserIds;
    UserResourceList accessibleUsers;
    for (const auto& userResource: users)
    {
        accessibleUserIds.insert(userResource->getId());
        accessibleUsers.append(userResource);
    }

    const auto isChildLayout=
        [accessProvider, accessibleUserIds, currentUserId](
            const QnResourcePtr& parent,
            const QnResourcePtr& child)
        {
            const auto user = parent.dynamicCast<QnUserResource>();
            const auto layout = child.dynamicCast<QnLayoutResource>();
            const auto wrongLayout = !layout || layout->flags().testFlag(Qn::local);
            if (wrongLayout || !accessProvider->hasAccess(user, layout))
                return false;

            const auto parentId = layout->getParentId();
            const auto userId = user->getId();
            return userId == parentId
                || (!accessibleUserIds.contains(parentId) && userId == currentUserId);
        };

    NodeList childNodes;
    for (const auto& userResource: accessibleUsers)
    {
        const auto node = createParentResourceNode(userResource,
            isChildLayout, createCheckableLayoutNode, Qt::Unchecked, childrenCountTextGenerator);
        if (node->childrenCount() > 0)
            childNodes.append(node);
    }

    return ViewNode::create(childNodes);
}

NodeViewStatePatch createCurrentUserLayoutsPatch()
{
    const auto userRoot =
        []()
        {
            const auto commonModule = qnClientCoreModule->commonModule();
            const auto userWatcher = commonModule->instance<nx::client::core::UserWatcher>();
            const auto currentUser = userWatcher->user();
            const auto root = createUserLayoutsNode(
                {currentUser}, ChildrenCountExtraTextGenerator());
            return root->children().first();
        }();
    return NodeViewStatePatch::fromRootNode(userRoot);
}

NodeViewStatePatch createParentedLayoutsPatch(
    const ChildrenCountExtraTextGenerator& childrenExtraTextGenerator)
{
    const auto commonModule = qnClientCoreModule->commonModule();
    const auto accessProvider = commonModule->resourceAccessProvider();
    const auto userWatcher = commonModule->instance<nx::client::core::UserWatcher>();
    const auto currentUser = userWatcher->user();

    const auto pool = qnClientCoreModule->commonModule()->resourcePool();

    const auto filterUser =
        [accessProvider, currentUser](const QnResourcePtr& resource)
        {
            return accessProvider->hasAccess(currentUser, resource.dynamicCast<QnUserResource>());
        };

    UserResourceList accessibleUsers;
    for (const auto& userResource: pool->getResources<QnUserResource>(filterUser))
        accessibleUsers.append(userResource);

    const auto root = createUserLayoutsNode(accessibleUsers, childrenExtraTextGenerator);
    return NodeViewStatePatch::fromRootNode(root);
}

}

namespace nx {
namespace client {
namespace desktop {

bool MultipleLayoutSelectionDialog::getLayouts(
    QWidget* parent,
    const QnResourceList& checkedLayouts,
    QnResourceList& resources)
{
    MultipleLayoutSelectionDialog dialog(checkedLayouts, parent);

    if (dialog.exec() != QDialog::Accepted)
        return false;

    const auto root = dialog.ui->layoutsTree->state().rootNode;
    resources = getSelectedResources(root);
    return true;
}

MultipleLayoutSelectionDialog::MultipleLayoutSelectionDialog(
    const QnResourceList& checkedLayouts,
    QWidget* parent)
    :
    base_type(parent),
    ui(new Ui::MultipleLayoutSelectionDialog)
{
    ui->setupUi(this);

    static const auto childrenCountExtratextGenerator =
        [](int count) { return lit("- %1").arg(tr("%n layouts", nullptr, count)); };

    const auto proxyModel = new AccessibleLayoutSortModel(resourceNameColumn, this);
    const auto tree = ui->layoutsTree;
    tree->setProxyModel(proxyModel);
    tree->applyPatch(createParentedLayoutsPatch(childrenCountExtratextGenerator));
//    tree->applyPatch(createCurrentUserLayoutsPatch());

    tree->applyPatch(ResourceNodeViewStateReducer::getLeafResourcesCheckedPatch(
        {resourceCheckColumn}, tree->state(), checkedLayouts));

    tree->expandAll();
    tree->setupHeader();

    connect(ui->searchLineEdit, &SearchLineEdit::textChanged, this,
        [proxyModel](const QString& text)
        {
            proxyModel->setFilter(text);
        });

    const auto scrollBar = new QnSnappedScrollBar(ui->scrollArea);
    scrollBar->setUseItemViewPaddingWhenVisible(true);
    ui->scrollArea->setVerticalScrollBar(scrollBar->proxyScrollBar());
}

MultipleLayoutSelectionDialog::~MultipleLayoutSelectionDialog()
{
}

} // namespace desktop
} // namespace client
} // namespace nx
