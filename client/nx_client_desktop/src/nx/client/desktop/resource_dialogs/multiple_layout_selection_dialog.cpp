#include "multiple_layout_selection_dialog.h"
#include "ui_multiple_layout_selection_dialog.h"

#include "layout_sort_proxy_model.h"

#include <nx/client/desktop/node_view/details/node_view_store.h>
#include <nx/client/desktop/node_view/details/node_view_state_patch.h>
#include <nx/client/desktop/node_view/details/node/view_node.h>
#include <nx/client/desktop/node_view/details/node/view_node_helpers.h>
#include <nx/client/desktop/node_view/resource_node_view/resource_node_view_constants.h>
#include <nx/client/desktop/node_view/resource_node_view/resource_view_node_helpers.h>

#include <common/common_module.h>
#include <client_core/client_core_module.h>
#include <client/client_settings.h>
#include <core/resource/user_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_access/providers/resource_access_provider.h>
#include <nx/client/core/watchers/user_watcher.h>
#include <ui/widgets/common/snapped_scrollbar.h>

#include <nx/client/desktop/node_view/selection_node_view/selection_node_view_state_reducer.h>
#include <nx/client/desktop/node_view/details/node/view_node_data_builder.h>

namespace {

using namespace nx::client::desktop::node_view;
using namespace nx::client::desktop::node_view::details;

using LayoutIdSet = QSet<QnUuid>;
using UserLayoutHash = QHash<QnUuid, LayoutIdSet>;
using CountTextGenerator = std::function<QString (int count)>;

enum TreeMode
{
    singleUserTreeMode,
    multipleUsersTreeMode
};

TreeMode calculateTreeMode(bool singleUser, bool forceAllLayoutsMode)
{
    if (singleUser)
        return singleUserTreeMode;

    return forceAllLayoutsMode
        ? multipleUsersTreeMode
        : static_cast<TreeMode>(qnSettings->layoutSelectionDialogTreeMode());
}

template<typename ResourceType>
QnSharedResourcePointerList<ResourceType> getAccessibleResources(
    const QnUserResourcePtr& subject,
    QnSharedResourcePointerList<ResourceType> resources,
    QnResourceAccessProvider* accessProvider)
{
    const auto itEnd = std::remove_if(resources.begin(), resources.end(),
        [accessProvider, subject](const QnSharedResourcePointer<ResourceType>& other)
        {
            return !accessProvider->hasAccess(subject, other);
        });
    resources.erase(itEnd, resources.end());
    return resources;
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
    QnCommonModule* commonModule,
    QnResourcePool* pool)
{
    UserLayoutHash result;

    const auto accessProvider = commonModule->resourceAccessProvider();
    const auto acessibleUsers = getAccessibleResources(
        currentUser, pool->getResources<QnUserResource>(), accessProvider);

    const auto acessibleLayouts = getAccessibleResources(
        currentUser, pool->getResources<QnLayoutResource>(), accessProvider);
    for (const auto user: acessibleUsers)
    {
        const auto userLayouts = getLayoutsForUser(user, acessibleLayouts, accessProvider);
        if (userLayouts.isEmpty())
            continue;

        auto& layoutsHash = result.insert(user->getId(), {}).value();
        for (const auto userLayout: userLayouts)
            layoutsHash.insert(userLayout->getId());
    }

    return result;
}

NodePtr createUserLayouts(
    const CountTextGenerator& countTextGenerator,
    const QnUserResourcePtr& parentUser,
    const LayoutIdSet& layoutIds,
    const QnResourcePool* pool)
{
    const auto userNode = createResourceNode(
        parentUser, countTextGenerator(layoutIds.size()), false);

    for (const auto id: layoutIds)
        userNode->addChild(createResourceNode(pool->getResourceById(id), QString(), true));
    return userNode;
}

NodePtr createAllLayoutsPatch(
    const CountTextGenerator& countTextGenerator,
    const UserLayoutHash& data,
    const QnResourcePool* pool)
{
    const auto root = ViewNode::create();
    for (auto it = data.begin(); it != data.end(); ++it)
    {
        const auto userResource = pool->getResourceById<QnUserResource>(it.key());
        root->addChild(createUserLayouts(countTextGenerator, userResource, it.value(), pool));
    }
    return root;
}

bool shouldForceAllLayoutsMode(
    const QnUuid& currentUserId,
    const UuidSet& selectedLayouts,
    const UserLayoutHash& layoutsData)
{
    const auto getSelectedLayouts =
        [selectedLayouts](UuidSet layoutIds) -> UuidSet
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

namespace nx {
namespace client {
namespace desktop {

struct MultipleLayoutSelectionDialog::Private: public QObject
{
    Private(
        const MultipleLayoutSelectionDialog& owner,
        const CountTextGenerator& generator,
        const UuidSet& selectedLayoutsHash);

    void updateModeSwitchAvailability();
    void handleSelectionChanged(const QnUuid& resourceId, Qt::CheckState checkedState);
    void updateSwitchState();
    void reloadViewData();
    void handleShowAllLayoutsSwitch(bool checked);
    void setMode(TreeMode value);

    const MultipleLayoutSelectionDialog& q;
    const CountTextGenerator countGenerator;
    const QnUserResourcePtr currentUser;
    const UserLayoutHash data;
    const bool singleUser = false;
    UuidSet selectedLayouts;
    bool forceAllLayoutsMode = false;
    TreeMode mode = singleUserTreeMode;
};

MultipleLayoutSelectionDialog::Private::Private(
    const MultipleLayoutSelectionDialog& owner,
    const CountTextGenerator& generator,
    const UuidSet& selectedLayouts)
    :
    q(owner),
    countGenerator(generator),
    currentUser(q.commonModule()->instance<nx::client::core::UserWatcher>()->user()),
    data(createUserLayoutHash(currentUser, q.commonModule(), q.resourcePool())),
    singleUser(data.count() <= 1),
    selectedLayouts(selectedLayouts),
    forceAllLayoutsMode(shouldForceAllLayoutsMode(currentUser->getId(), selectedLayouts, data)),
    mode(calculateTreeMode(singleUser, forceAllLayoutsMode))
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
            NX_EXPECT(false, "Should not get here!");
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
    q.ui->showAllLayoutSwitch->setDisabled(singleUser || forceAllLayoutsMode);
}

void MultipleLayoutSelectionDialog::Private::updateSwitchState()
{
    qnSettings->setlayoutSelectionDialogTreeMode(mode);
    q.ui->showAllLayoutSwitch->setChecked(mode == multipleUsersTreeMode);
}

void MultipleLayoutSelectionDialog::Private::reloadViewData()
{
    auto& view = *q.ui->layoutsTree;

    const auto pool = q.resourcePool();
    const auto root = mode == singleUserTreeMode
        ? createUserLayouts(countGenerator, currentUser, data[currentUser->getId()], pool)
        : createAllLayoutsPatch(countGenerator, data, pool);
    if (view.state().rootNode)
        view.applyPatch(NodeViewStatePatch::clearNodeView());
    view.applyPatch(NodeViewStatePatch::fromRootNode(root));
    view.setLeafResourcesSelected(selectedLayouts, true);

    view.expandAll();
}

//-------------------------------------------------------------------------------------------------

bool MultipleLayoutSelectionDialog::selectLayouts(
    UuidSet& selectedLayouts,
    QWidget* parent)
{
    MultipleLayoutSelectionDialog dialog(selectedLayouts, parent);

    if (dialog.exec() != QDialog::Accepted)
        return false;

    selectedLayouts = dialog.d->selectedLayouts;
    return true;
}

MultipleLayoutSelectionDialog::MultipleLayoutSelectionDialog(
    const UuidSet& selectedLayouts,
    QWidget* parent)
    :
    base_type(parent),
    d(new Private(
        *this,
        [](int count) { return lit("- %1").arg(tr("%n layouts", nullptr, count)); },
        selectedLayouts)),
    ui(new Ui::MultipleLayoutSelectionDialog)
{
    ui->setupUi(this);

    d->updateSwitchState();

    const auto proxyModel = new LayoutSortProxyModel(d->currentUser->getId(), this);
    const auto tree = ui->layoutsTree;
    tree->setProxyModel(proxyModel);
    tree->setupHeader();
    tree->setSelectionMode(ResourceSelectionNodeView::selectEqualResourcesMode);

    ui->stackedWidget->setCurrentWidget(ui->layoutsPage);
    connect(ui->searchLineEdit, &SearchLineEdit::textChanged, this,
        [this, proxyModel, tree](const QString& text)
        {
            proxyModel->setFilter(text, NodeViewBaseSortModel::LeafNodeFilterScope);
            const auto page = tree->model()->rowCount(QModelIndex())
                ? ui->layoutsPage
                : ui->notificationPage;
            ui->stackedWidget->setCurrentWidget(page);
        });

    connect(ui->showAllLayoutSwitch, &QPushButton::toggled, this,
        [this](bool checked)
        {
            d->mode = (checked ? multipleUsersTreeMode : singleUserTreeMode);
            d->reloadViewData();
        });

    connect(ui->layoutsTree, &ResourceSelectionNodeView::resourceSelectionChanged,
        d, &Private::handleSelectionChanged);

    const auto scrollBar = new QnSnappedScrollBar(ui->scrollArea);
    scrollBar->setUseItemViewPaddingWhenVisible(true);
    ui->scrollArea->setVerticalScrollBar(scrollBar->proxyScrollBar());

    d->reloadViewData();
}

MultipleLayoutSelectionDialog::~MultipleLayoutSelectionDialog()
{
}

} // namespace desktop
} // namespace client
} // namespace nx
