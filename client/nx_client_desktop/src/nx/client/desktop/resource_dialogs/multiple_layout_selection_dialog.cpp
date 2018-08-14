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

template<typename ResourceType>
QnSharedResourcePointerList<ResourceType> getAccessibleResources(
    QnSharedResourcePointerList<ResourceType> resources,
    const QnUserResourcePtr& subject,
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

UserLayoutHash createUserLayoutHash(
    const QnUserResourcePtr& currentUser,
    QnCommonModule* commonModule,
    QnResourcePool* pool)
{
    UserLayoutHash result;

    const auto accessProvider = commonModule->resourceAccessProvider();
    const auto acessibleUsers = getAccessibleResources<QnUserResource>(
        pool->getResources<QnUserResource>(), currentUser, accessProvider);

    const auto layouts = pool->getResources<QnLayoutResource>();
    for (const auto user: acessibleUsers)
    {
        const auto userLayouts = getAccessibleResources(layouts, user, accessProvider);
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

}

namespace nx {
namespace client {
namespace desktop {

struct MultipleLayoutSelectionDialog::Private
{
    Private(
        const MultipleLayoutSelectionDialog& owner,
        const CountTextGenerator& generator,
        const UuidSet& selectedLayoutsHash);

    const MultipleLayoutSelectionDialog& q;
    const CountTextGenerator countGenerator;
    const QnUserResourcePtr currentUser;
    const UserLayoutHash layoutsData;
    UuidSet selectedLayouts;
    bool allLayoutsMode = false;

    void reloadViewData();
};

MultipleLayoutSelectionDialog::Private::Private(
    const MultipleLayoutSelectionDialog& owner,
    const CountTextGenerator& generator,
    const UuidSet& selectedLayouts)
    :
    q(owner),
    countGenerator(generator),
    currentUser(q.commonModule()->instance<nx::client::core::UserWatcher>()->user()),
    layoutsData(createUserLayoutHash(currentUser, q.commonModule(), q.resourcePool())),
    selectedLayouts(selectedLayouts)
{
}

void MultipleLayoutSelectionDialog::Private::reloadViewData()
{
    auto& view = *q.ui->layoutsTree;

    const auto pool = q.resourcePool();
    const auto root = allLayoutsMode
        ? createAllLayoutsPatch(countGenerator, layoutsData, pool)
        : createUserLayouts(countGenerator, currentUser, layoutsData[currentUser->getId()], pool);
    if (view.state().rootNode)
        view.applyPatch(NodeViewStatePatch::clearNodeView());
    view.applyPatch(NodeViewStatePatch::fromRootNode(root));
    view.setResourcesSelected(selectedLayouts, true);

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

    const auto proxyModel = new LayoutSortProxyModel(d->currentUser->getId(), this);
    const auto tree = ui->layoutsTree;
    tree->setProxyModel(proxyModel);
    tree->setupHeader();

    connect(ui->searchLineEdit, &SearchLineEdit::textChanged, this,
        [proxyModel](const QString& text)
        {
            proxyModel->setFilter(text, NodeViewBaseSortModel::LeafNodeFilterScope);
        });

    connect(ui->showAllLayoutSwitch, &QPushButton::toggled, this,
        [this](bool allLayoutsMode)
        {
            d->allLayoutsMode = allLayoutsMode;
            d->reloadViewData();
        });

    connect(ui->layoutsTree, &ResourceNodeView::resourceSelectionChanged, this,
        [this](const QnUuid& resourceId, Qt::CheckState checkedState)
        {
            switch(checkedState)
            {
                case Qt::Checked:
                    d->selectedLayouts.insert(resourceId);
                    break;
                case Qt::Unchecked:
                    d->selectedLayouts.remove(resourceId);
                    break;
                default:
                    NX_EXPECT(false, "Should not get here!");
                    break;
            }
        });


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
