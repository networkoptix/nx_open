// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "grouped_resources_view_state_keeper.h"

#include <vector>

#include <QtCore/QString>
#include <QtCore/QAbstractItemModel>
#include <QtCore/QAbstractProxyModel>
#include <QtCore/QItemSelectionModel>
#include <QtCore/QPropertyAnimation>

#include <QtWidgets/QAction>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QAbstractScrollArea>
#include <QtWidgets/QScrollBar>

#include <nx/utils/log/assert.h>
#include <client/client_globals.h>
#include <ui/workbench/workbench_context.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_grouping/resource_grouping.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>

namespace {

// Lowermost source model in front of the chain, view model at the end.
std::vector<const QAbstractItemModel*> retrieveProxyChain(const QAbstractItemModel* viewModel)
{
    std::vector<const QAbstractItemModel*> result;
    auto proxyModel = qobject_cast<const QAbstractProxyModel*>(viewModel);
    while (proxyModel)
    {
        result.push_back(proxyModel);
        if (proxyModel->sourceModel()
            && !qobject_cast<const QAbstractProxyModel*>(proxyModel->sourceModel()))
        {
            result.push_back(proxyModel->sourceModel());
            break;
        }
        proxyModel = qobject_cast<const QAbstractProxyModel*>(proxyModel->sourceModel());
    }

    std::reverse(std::begin(result), std::end(result));

    return result;
}

bool indexesAreValid(const std::vector<QPersistentModelIndex>& persistentIndexes)
{
    return std::all_of(std::cbegin(persistentIndexes), std::cend(persistentIndexes),
        [](const auto& index) { return index.isValid(); });
}

QModelIndex getGroupIndex(const std::vector<QPersistentModelIndex>& persistentResourceIndexes)
{
    if (persistentResourceIndexes.empty())
        return QModelIndex();

    QModelIndex result = persistentResourceIndexes.back().parent();
    for (const auto& index: persistentResourceIndexes)
    {
        if (result == index)
            result == index.parent();
    }

    return result;
}

void scrollVerticallyByPixelDelta(const QAbstractScrollArea* scrollArea, int pixelDelta)
{
    static constexpr int kAnimationDurationMs = 200;
    static constexpr auto kEasingCurveType = QEasingCurve::InOutCubic;
    static constexpr auto kAnimationName = "scrollVerticallyByPixelDelta";

    const auto scrollBar = scrollArea->verticalScrollBar();
    if (!scrollBar)
        return;

    const auto newValue =
        std::clamp(scrollBar->minimum(), scrollBar->value() + pixelDelta, scrollBar->maximum());

    const auto childrenAnimations = scrollBar->findChildren<QAbstractAnimation*>(kAnimationName);
    for (auto animation: childrenAnimations)
        animation->stop();

    auto animation = new QPropertyAnimation(scrollBar, "value");

    animation->setObjectName(kAnimationName);
    animation->setStartValue(scrollBar->value());
    animation->setEndValue(newValue);
    animation->setDuration(kAnimationDurationMs);
    animation->setEasingCurve(QEasingCurve(kEasingCurveType));

    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

} // namespace

namespace nx::vms::client::desktop {

using NodeType = ResourceTree::NodeType;

GroupedResourcesViewStateKeeper::GroupedResourcesViewStateKeeper(
    QnWorkbenchContext* workbenchContext,
    QTreeView* treeView,
    QAbstractScrollArea* scrollArea,
    QAbstractItemModel* sourceModel)
    :
    base_type(),
    m_workbenchContext(workbenchContext),
    m_treeView(treeView),
    m_scrollArea(scrollArea),
    m_sourceModel(sourceModel)
{
    if (!m_workbenchContext || !m_treeView || !m_scrollArea || !m_sourceModel)
    {
        NX_ASSERT(false, "Invalid parameter");
        return;
    }

    using namespace ui::action;

    m_connections.add(
        connect(workbenchContext->action(CreateNewCustomGroupAction), &QAction::triggered,
        this, &GroupedResourcesViewStateKeeper::commonResourceTreeGroupPreActionHandler));
    m_connections.add(
        connect(workbenchContext->action(NewCustomGroupCreatedEvent), &QAction::triggered,
        this, &GroupedResourcesViewStateKeeper::onNewResourceTreeGroupCreated));

    m_connections.add(
        connect(workbenchContext->action(AssignCustomGroupIdAction), &QAction::triggered,
        this, &GroupedResourcesViewStateKeeper::commonResourceTreeGroupPreActionHandler));
    m_connections.add(
        connect(workbenchContext->action(CustomGroupIdAssignedEvent), &QAction::triggered,
        this, &GroupedResourcesViewStateKeeper::onResourceTreeGroupIdAssigned));

    m_connections.add(
        connect(workbenchContext->action(RenameCustomGroupAction), &QAction::triggered,
        this, &GroupedResourcesViewStateKeeper::commonResourceTreeGroupPreActionHandler));
    m_connections.add(
        connect(workbenchContext->action(CustomGroupRenamedEvent), &QAction::triggered,
        this, &GroupedResourcesViewStateKeeper::onResourceTreeGroupRenamed));

    m_connections.add(
        connect(workbenchContext->action(RemoveCustomGroupAction), &QAction::triggered,
        this, &GroupedResourcesViewStateKeeper::commonResourceTreeGroupPreActionHandler));
    m_connections.add(
        connect(workbenchContext->action(CustomGroupRemovedEvent), &QAction::triggered,
        this, &GroupedResourcesViewStateKeeper::commonResourceTreeGroupPostEventHandler));

    m_connections.add(
        connect(workbenchContext->action(MoveToCustomGroupAction), &QAction::triggered,
        this, &GroupedResourcesViewStateKeeper::commonResourceTreeGroupPreActionHandler));
    m_connections.add(
        connect(workbenchContext->action(MovedToCustomGroupEvent), &QAction::triggered,
        this, &GroupedResourcesViewStateKeeper::commonResourceTreeGroupPostEventHandler));
}

void GroupedResourcesViewStateKeeper::commonResourceTreeGroupPreActionHandler()
{
    saveViewStateRestoreData(m_workbenchContext->menu()->currentParameters(sender()).resources());
}

void GroupedResourcesViewStateKeeper::commonResourceTreeGroupPostEventHandler()
{
    restoreExpandedStateFromStoredData();
    clearViewStateRestoreData();
}

void GroupedResourcesViewStateKeeper::onNewResourceTreeGroupCreated()
{
    const auto parameters = actionManager()->currentParameters(sender());
    const auto newCompositeGroupId =
        parameters.argument(Qn::ResourceTreeCustomGroupIdRole).toString();

    if (m_visibleResourceIndexes.empty())
        return;

    auto parentIndex = m_visibleResourceIndexes.front();
    while (parentIndex.isValid())
    {
        const auto compositeGroupId = parentIndex.data(Qn::ResourceTreeCustomGroupIdRole).toString();
        const auto nodeType = parentIndex.data(Qn::NodeTypeRole).value<ResourceTree::NodeType>();

        if ((compositeGroupId.compare(newCompositeGroupId, Qt::CaseInsensitive) == 0)
            && (nodeType == ResourceTree::NodeType::customResourceGroup))
        {
            makeCurrent(parentIndex);
            treeView()->edit(toViewIndex(parentIndex));
        }
        parentIndex = parentIndex.parent();
    }

    restoreExpandedStateFromStoredData();
    clearViewStateRestoreData();
}

void GroupedResourcesViewStateKeeper::onResourceTreeGroupRenamed()
{
    restoreExpandedStateFromStoredData();
    const auto selectionModel = m_treeView->selectionModel();

    if (!selectionModel->selection().isEmpty())
    {
        // Another item has been selected while group renaming, keep item position in item view.
        const auto currentIndex = selectionModel->currentIndex();
        if (selectionModel->selection().contains(currentIndex))
        {
            const auto currentIndexRect = m_treeView->visualRect(currentIndex);

            if (m_initialCurrentItemRect.isValid() && currentIndexRect.isValid())
            {
                const auto verticalPixelDelta =
                    currentIndexRect.center().y() - m_initialCurrentItemRect.center().y();
                scrollVerticallyByPixelDelta(m_scrollArea, verticalPixelDelta);
            }
        }
    }
    else if (indexesAreValid(m_affectedResourceIndexes))
    {
        const auto model = m_affectedResourceIndexes.back().model();
        const auto groupIndex = getGroupIndex(m_affectedResourceIndexes);

        if (!m_treeView->hasFocus())
            m_treeView->setFocus(Qt::OtherFocusReason);
        selectionModel->clear();

        if (model->rowCount(groupIndex) > m_affectedResourceIndexes.size())
        {
            // Group has been merged into another after renaming, select original group resources.
            const auto begin = std::cbegin(m_affectedResourceIndexes);
            const auto end = std::cend(m_affectedResourceIndexes);

            selectionModel->select(toViewIndex(*begin), QItemSelectionModel::SelectCurrent);
            for (auto itr = std::next(begin); itr != end; ++itr)
                selectionModel->select(toViewIndex(*itr), QItemSelectionModel::Select);
        }
        else
        {
            // Group has been simply renamed, keep group selected state and position in item view.
            selectionModel->select(toViewIndex(groupIndex), QItemSelectionModel::SelectCurrent);
            const auto currentIndexRect = m_treeView->visualRect(toViewIndex(groupIndex));

            if (m_initialCurrentItemRect.isValid() && currentIndexRect.isValid())
            {
                const auto verticalPixelDelta =
                    currentIndexRect.center().y() - m_initialCurrentItemRect.center().y();
                scrollVerticallyByPixelDelta(m_scrollArea, verticalPixelDelta);
            }
        }
    }

    clearViewStateRestoreData();
}

void GroupedResourcesViewStateKeeper::onResourceTreeGroupIdAssigned()
{
    commonResourceTreeGroupPostEventHandler();
}

QModelIndex GroupedResourcesViewStateKeeper::toSourceIndex(const QModelIndex& index) const
{
    if (!index.isValid())
        return {};

    if (index.model() == m_sourceModel)
        return index;

    if (const auto proxyModel = qobject_cast<const QAbstractProxyModel*>(index.model()))
        return toSourceIndex(proxyModel->mapToSource(index));

    NX_ASSERT(false, "Unexpected parameter");
    return {};
}

QModelIndex GroupedResourcesViewStateKeeper::toViewIndex(const QModelIndex& index) const
{
    if (!index.isValid())
        return {};

    const auto viewModel = treeView()->model();
    if (index.model() == viewModel)
        return index;

    const auto proxyChain = retrieveProxyChain(viewModel);
    auto proxyItr = std::find(std::cbegin(proxyChain), std::cend(proxyChain), index.model());
    if (proxyItr == std::cend(proxyChain))
    {
        NX_ASSERT(false, "Unexpected parameter");
        return {};
    }

    QModelIndex result = index;
    for (auto itr = std::next(proxyItr); itr != proxyChain.end(); ++itr)
        result = static_cast<const QAbstractProxyModel*>(*itr)->mapFromSource(result);

    return result;
}

void GroupedResourcesViewStateKeeper::makeCurrent(const QModelIndex& index)
{
    const auto viewIndex = toViewIndex(index);
    m_treeView->selectionModel()->clear();

    m_treeView->selectionModel()->setCurrentIndex(viewIndex,
        {QItemSelectionModel::Select, QItemSelectionModel::Current});
}

bool GroupedResourcesViewStateKeeper::isVisibleByExpandedState(const QModelIndex& index) const
{
    if (!index.isValid())
    {
        NX_ASSERT(false, "Invalid null parameter");
        return false;
    }

    const auto viewIndex = toViewIndex(index);
    if (!viewIndex.isValid())
        return false;

    auto parentViewIndex = viewIndex.parent();
    while (parentViewIndex.isValid())
    {
        if (!treeView()->isExpanded(parentViewIndex))
            return false;
        parentViewIndex = parentViewIndex.model()->parent(parentViewIndex);
    }

    return true;
}

void GroupedResourcesViewStateKeeper::expandToMakeVisible(const QModelIndex& index) const
{
    if (!index.isValid())
        return;

    const auto viewIndex = toViewIndex(index);
    if (!viewIndex.isValid())
        return;

    auto parentViewIndex = viewIndex.parent();
    while (parentViewIndex.isValid())
    {
        if (!treeView()->isExpanded(parentViewIndex))
            treeView()->expand(parentViewIndex);
        parentViewIndex = parentViewIndex.model()->parent(parentViewIndex);
    }
}

void GroupedResourcesViewStateKeeper::saveViewStateRestoreData(const QnResourceList& resources)
{
    for (const auto& resource: resources)
    {
        // TODO: #vbreus Resource index may be obtained with O(1) complexity. Current
        // implementation is far from optimal.
        QModelIndexList resourceIndexes = m_sourceModel->match(
            m_sourceModel->index(0, 0),
            Qn::ResourceRole,
            QVariant::fromValue<QnResourcePtr>(resource),
            1,
            Qt::MatchFlags({Qt::MatchExactly, Qt::MatchRecursive}));

        if (resourceIndexes.isEmpty())
            continue;

        const auto resourceIndex = resourceIndexes.first();
        m_affectedResourceIndexes.push_back(resourceIndex);

        if (isVisibleByExpandedState(toViewIndex(resourceIndex)))
            m_visibleResourceIndexes.push_back(resourceIndex);
    }

    const auto selectionModel = m_treeView->selectionModel();
    const auto currentIndex = selectionModel->currentIndex();
    if (currentIndex.isValid() && selectionModel->selection().contains(currentIndex))
        m_initialCurrentItemRect = m_treeView->visualRect(currentIndex);
    else
        m_initialCurrentItemRect = QRect();
}

void GroupedResourcesViewStateKeeper::restoreExpandedStateFromStoredData()
{
    for (const auto& index: m_visibleResourceIndexes)
    {
        if (index.isValid())
            expandToMakeVisible(index);
    }
}

void GroupedResourcesViewStateKeeper::clearViewStateRestoreData()
{
    m_affectedResourceIndexes.clear();
    m_visibleResourceIndexes.clear();
    m_initialCurrentItemRect = QRect();
}

QTreeView* GroupedResourcesViewStateKeeper::treeView() const
{
    return m_treeView;
}

ui::action::Manager* GroupedResourcesViewStateKeeper::actionManager() const
{
    return m_workbenchContext->menu();
}

} // namespace nx::vms::client::desktop
