#include "multiple_layout_selection_dialog.h"
#include "ui_multiple_layout_selection_dialog.h"

#include <nx/client/desktop/resource_views/node_view/node_view_model.h>
#include <nx/client/desktop/resource_views/node_view/node_view_state.h>
#include <nx/client/desktop/resource_views/node_view/node_view_state_patch.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node_helpers.h>
#include <nx/client/desktop/resource_views/node_view/resource_node_view/resource_node_view_state_reducer.h>
#include <nx/client/desktop/resource_views/layout/accessible_layout_sort_model.h>

#include <common/common_module.h>
#include <client_core/client_core_module.h>
#include <core/resource/user_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_access/providers/resource_access_provider.h>
#include <nx/client/core/watchers/user_watcher.h>

namespace {

using namespace nx::client::desktop;
using namespace nx::client::desktop::helpers;

NodeViewStatePatch testPatch()
{
    using namespace helpers;

    return NodeViewStatePatch::fromRootNode(ViewNode::create({
        createCheckAllNode(lit("Check All"), QIcon(), -2),
        createSeparatorNode(-1),
        createNode(lit("1"), {
            createCheckAllNode(lit("Check All #1"), QIcon(), -2),
            createSeparatorNode(-1),
            createNode(lit("1_2")),
            createNode(lit("1_1")),
            createSeparatorNode(1),
            createCheckAllNode(lit("Check All #1"), QIcon(), 2),
            }),
        createNode(lit("2")),
        createNode(lit("3"), 2),
    }));
}

//-------------------------------------------------------------------------------------------------

const auto createCheckableLayoutNode =
    [](const QnResourcePtr& resource) -> NodePtr
    {
        return helpers::createResourceNode(resource, Qt::Unchecked);
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
        const auto node = helpers::createParentResourceNode(userResource,
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

    resources = helpers::getLeafSelectedResources(dialog.ui->layoutsTree->state().rootNode);
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
        [](int count)
        {
            return lit("- %1").arg(tr("%n layouts", nullptr, count));
        };

    const auto proxyModel = new AccessibleLayoutSortModel(this);
    const auto tree = ui->layoutsTree;
    tree->setProxyModel(proxyModel);
//    tree->applyPatch(testPatch());
    tree->applyPatch(createParentedLayoutsPatch(childrenCountExtratextGenerator));
//    tree->applyPatch(createCurrentUserLayoutsPatch());

    tree->applyPatch(ResourceNodeViewStateReducer::getLeafResourcesCheckedPatch(
        tree->state(), checkedLayouts));

    tree->expandAll();
    connect(ui->searchLineEdit, &SearchLineEdit::textChanged, this,
        [proxyModel](const QString& text)
        {
            qWarning() << "set filter: " << text;
            proxyModel->setFilter(text);
        });
}

MultipleLayoutSelectionDialog::~MultipleLayoutSelectionDialog()
{
}

} // namespace desktop
} // namespace client
} // namespace nx
