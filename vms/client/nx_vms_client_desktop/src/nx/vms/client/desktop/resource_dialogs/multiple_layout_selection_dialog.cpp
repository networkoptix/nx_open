#include "multiple_layout_selection_dialog.h"
#include "ui_multiple_layout_selection_dialog.h"

#include "layout_sort_proxy_model.h"

#include <nx/vms/client/desktop/node_view/details/node_view_store.h>
#include <nx/vms/client/desktop/node_view/details/node_view_state_patch.h>
#include <nx/vms/client/desktop/node_view/details/node/view_node.h>
#include <nx/vms/client/desktop/node_view/details/node/view_node_helpers.h>
#include <nx/vms/client/desktop/node_view/resource_node_view/resource_node_view_constants.h>
#include <nx/vms/client/desktop/node_view/resource_node_view/resource_view_node_helpers.h>
#include <nx/vms/client/desktop/node_view/resource_node_view/resource_selection_node_view.h>

#include <common/common_module.h>
#include <client_core/client_core_module.h>
#include <client/client_settings.h>
#include <core/resource/user_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_access/providers/resource_access_provider.h>

#include <nx/vms/client/desktop/node_view/details/node/view_node_data_builder.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/workbench/workbench_context.h>
#include <nx_ec/access_helpers.h>

namespace {

using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::node_view;
using namespace nx::vms::client::desktop::node_view::details;

using LayoutIdSet = QSet<QnUuid>;
using UserLayoutHash = QHash<QnUuid, LayoutIdSet>;

bool isAllLayoutsMode(bool singleUser, bool forceAllLayoutsMode)
{
    if (singleUser)
        return false;

    return forceAllLayoutsMode || qnSettings->allLayoutsSelectionDialogMode();
}


QnLayoutResourceList getLayoutsForUser(
    const QnUserResourcePtr& user,
    const QnLayoutResourceList& layouts,
    QnResourceAccessProvider* accessProvider)
{
    const auto userId = user->getId();
    QnLayoutResourceList result;
    for (const auto layout: layouts)
    {
        if (layout->flags().testFlag(Qn::local))
            continue; //< We don't show not saved layouts.

        if (layout->getParentId() == userId
            || (accessProvider->hasAccess(user, layout) && layout->isShared()))
        {
            result.append(layout);
        }
    }

    return result;
}

UserLayoutHash createUserLayoutHash(
    const QnUserResourcePtr& currentUser,
    QnCommonModule* commonModule)
{
    UserLayoutHash result;

    using namespace ec2::access_helpers;
    const auto pool = commonModule->resourcePool();
    const auto accessProvider = commonModule->resourceAccessProvider();
    const auto acessibleUsers = getAccessibleResources(
        currentUser, pool->getResources<QnUserResource>(), accessProvider);

    const auto accessibleLayouts = getAccessibleResources(
        currentUser, pool->getResources<QnLayoutResource>(), accessProvider);
    for (const auto user: acessibleUsers)
    {
        const auto userLayouts = getLayoutsForUser(user, accessibleLayouts, accessProvider);
        if (userLayouts.isEmpty())
            continue;

        auto& layoutsHash = result.insert(user->getId(), {}).value();
        for (const auto userLayout: userLayouts)
            layoutsHash.insert(userLayout->getId());
    }

    return result;
}

NodePtr createUserLayouts(
    const QnUserResourcePtr& parentUser,
    const LayoutIdSet& layoutIds,
    const QnResourcePool* pool)
{
    const auto extraText = lit("- %1").arg(
        MultipleLayoutSelectionDialog::tr("%n layouts", nullptr, layoutIds.size()));

    const auto userNode = createResourceNode(parentUser, extraText, false);

    for (const auto id: layoutIds)
        userNode->addChild(createResourceNode(pool->getResourceById(id), QString(), true));
    return userNode;
}

NodePtr createAllLayoutsPatch(
    const UserLayoutHash& data,
    const QnResourcePool* pool)
{
    const auto root = ViewNode::create();
    for (auto it = data.begin(); it != data.end(); ++it)
    {
        const auto userResource = pool->getResourceById<QnUserResource>(it.key());
        if (!userResource)
            continue; //< User can be deleted just after dialog raise.

        root->addChild(createUserLayouts(userResource, it.value(), pool));
    }
    return root;
}

bool shouldForceAllLayoutsMode(
    const QnUuid& currentUserId,
    const QnUuidSet& selectedLayouts,
    const UserLayoutHash& layoutsData)
{
    const auto getSelectedLayouts =
        [selectedLayouts](QnUuidSet layoutIds) -> QnUuidSet
        {
            return layoutIds.intersect(selectedLayouts);
        };

    const auto currentUserSelectedLayouts = getSelectedLayouts(layoutsData[currentUserId]);
    for (auto it = layoutsData.begin(); it != layoutsData.end(); ++it)
    {
        const auto userId = it.key();
        if (userId == currentUserId)
            continue; //< Skips layouts of current user - they are visible in any case.

        auto anotherUserSelectedLayouts = getSelectedLayouts(it.value());
        anotherUserSelectedLayouts.subtract(currentUserSelectedLayouts);
        if (!anotherUserSelectedLayouts.isEmpty())
            return true;
    }
    return false;
}

}

namespace nx::vms::client::desktop {

struct MultipleLayoutSelectionDialog::Private: public QObject
{
    Private(
        const MultipleLayoutSelectionDialog* owner,
        const QnUuidSet& selectedLayoutsHash);

    void updateModeSwitchAvailability();
    void handleSelectionChanged(const QnUuid& resourceId, Qt::CheckState checkedState);
    void reloadViewData();
    void handleShowAllLayoutsSwitch(bool checked);

    const MultipleLayoutSelectionDialog* q = nullptr;
    const QnUserResourcePtr currentUser;
    const UserLayoutHash data;
    const bool singleUser = false;
    QnUuidSet selectedLayouts;
    bool forceAllLayoutsMode = false;
    bool allLayoutsMode = false;
};

MultipleLayoutSelectionDialog::Private::Private(
    const MultipleLayoutSelectionDialog* owner,
    const QnUuidSet& selectedLayouts)
    :
    q(owner),
    currentUser(q->context()->user()),
    data(createUserLayoutHash(currentUser, q->commonModule())),
    singleUser(data.count() <= 1),
    selectedLayouts(selectedLayouts),
    forceAllLayoutsMode(shouldForceAllLayoutsMode(currentUser->getId(), selectedLayouts, data)),
    allLayoutsMode(isAllLayoutsMode(singleUser, forceAllLayoutsMode))
{
}

void MultipleLayoutSelectionDialog::Private::handleSelectionChanged(
    const QnUuid& resourceId,
    Qt::CheckState checkedState)
{
    switch(checkedState)
    {
        case Qt::Checked:
            selectedLayouts.insert(resourceId);
            break;
        case Qt::Unchecked:
            selectedLayouts.remove(resourceId);
            break;
        default:
            NX_ASSERT(false, "Should not get here!");
            break;
    }

    updateModeSwitchAvailability();
}

void MultipleLayoutSelectionDialog::Private::updateModeSwitchAvailability()
{
    if (singleUser)
        return; //< Single user is final marker for the mode.

    const auto currentUserId = currentUser->getId();
    const bool forceAllLayouts = shouldForceAllLayoutsMode(currentUserId, selectedLayouts, data);
    if (forceAllLayouts == forceAllLayoutsMode)
        return;

    forceAllLayoutsMode = forceAllLayouts;
    q->ui->showAllLayoutSwitch->setDisabled(singleUser || forceAllLayoutsMode);
}

void MultipleLayoutSelectionDialog::Private::reloadViewData()
{
    const auto view = q->ui->filteredResourceSelectionWidget->view();

    const auto pool = q->resourcePool();
    const auto root = allLayoutsMode
        ? createAllLayoutsPatch(data, pool)
        : createUserLayouts(currentUser, data[currentUser->getId()], pool);
    if (view->state().rootNode)
        view->applyPatch(NodeViewStatePatch::clearNodeView());
    view->applyPatch(NodeViewStatePatch::fromRootNode(root));
    view->setLeafResourcesSelected(selectedLayouts, true);

    view->expandAll();
}

//-------------------------------------------------------------------------------------------------

bool MultipleLayoutSelectionDialog::selectLayouts(
    QnUuidSet& selectedLayouts,
    QWidget* parent)
{
    MultipleLayoutSelectionDialog dialog(selectedLayouts, parent);

    if (dialog.d->data.isEmpty())
    {
        QnMessageBox::warning(parent, tr("You do not have any layouts"));
        return false;
    }

    if (dialog.exec() != QDialog::Accepted)
        return false;

    selectedLayouts = dialog.d->selectedLayouts;
    return true;
}

MultipleLayoutSelectionDialog::MultipleLayoutSelectionDialog(
    const QnUuidSet& selectedLayouts,
    QWidget* parent)
    :
    base_type(parent),
    d(new Private(this, selectedLayouts)),
    ui(new Ui::MultipleLayoutSelectionDialog())
{
    ui->setupUi(this);

    ui->showAllLayoutSwitch->setChecked(d->allLayoutsMode);

    const auto proxyModel = new LayoutSortProxyModel(d->currentUser->getId(), this);
    const auto view = ui->filteredResourceSelectionWidget->view();
    view->setProxyModel(proxyModel);
    view->setupHeader();
    view->setSelectionMode(ResourceSelectionNodeView::selectEqualResourcesMode);

    connect(ui->showAllLayoutSwitch, &QPushButton::toggled, this,
        [this](bool checked)
        {
            d->allLayoutsMode = checked;
            qnSettings->setAllLayoutsSelectionDialogMode(d->allLayoutsMode);
            d->reloadViewData();
        });

    connect(view, &ResourceSelectionNodeView::resourceSelectionChanged,
        d, &Private::handleSelectionChanged);

    d->reloadViewData();
}

MultipleLayoutSelectionDialog::~MultipleLayoutSelectionDialog()
{
}

} // namespace nx::vms::client::desktop
