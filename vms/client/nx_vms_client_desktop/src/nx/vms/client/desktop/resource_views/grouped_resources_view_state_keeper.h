// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

class QAbstractItemModel;
class QAbstractScrollArea;
class QTreeView;

class QnWorkbenchContext;
namespace nx::vms::client::desktop::ui::action { class Manager; }

#include <QtCore/QModelIndex>
#include <QtCore/QRect>
#include <QtCore/QSet>

#include <nx/utils/scoped_connections.h>
#include <core/resource/resource.h>

namespace nx::vms::client::desktop {

/**
 * Number of kinds of group nodes in the Resource Tree model (particularly ones that come from
 * GroupingEntity) have transient nature. They are not result of consolidated grouping data mapping
 * and there are no 'group' objects anywhere. These groups are obtained by extracting common
 * features from resource collections and can't exist without presence of originate resources.
 * This approach have plenty of benefits, but at the same time it have certain drawbacks: when it
 * comes to preserving item view state during model transformations, specifically: expanded state
 * and selection, general approach with persistent model indexes are not applicable here a-priory.
 * By the other hand, groups contents - resource items which spawn these groups are never lose
 * tracking, unique by nature and basically almost always can be accessed by forever valid
 * persistent index. So, sole purpose of this class is deducing view representation details for
 * such groups from its originators. Say, instead groups collapsed state, we may deal with
 * visibility state of resources, which is basically the same information, but from reverse
 * direction, etc. This class have no other responsibilities and specializations, contain no
 * business logic and operates only view related data.
 */
class GroupedResourcesViewStateKeeper: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:

    /**
     * @param workbenchContext Valid pointer to workbench context.
     * @param treeView View which use sourceModel as data source, directly or not.
     * @param scrollArea Scroll area which contains tree view passed as parameter.
     * @param sourceModel Pointer to the lowermost source model expected.
     */
    GroupedResourcesViewStateKeeper(
        QnWorkbenchContext* workbenchContext,
        QTreeView* treeView,
        QAbstractScrollArea* scrollArea,
        QAbstractItemModel* sourceModel);

private:

    /**
     * Handler that executed before all custom group interaction actions. Stores persistent indexes
     * of resource items which: 1) will be affected by upcoming action; 2) are visible are visible
     * in terms of view collapsed state. As actions will be performed, these indexes will be used
     * to restore tree view collapsed state.
     */
    void commonResourceTreeGroupPreActionHandler();

    /**
     * Handler that executed after custom group interaction actions which don't have extra effects
     * on tree view. Just restores tree view collapsed state. Holds for 'Remove Group' and 'Move
     * Group' actions.
     */
    void commonResourceTreeGroupPostEventHandler();

    /**
     * Handler that executed after new group creation. Besides restoring collapsed state, manages
     * current item change and engaging edit mode.
     */
    void onNewResourceTreeGroupCreated();

    /**
     * Handler that executed after group ID assignment (non user action). TODO: #vbreus Special
     * actions while transition to the groups on the other server expected to be here.
     */
    void onResourceTreeGroupIdAssigned();

    /**
     * Handler that executed after group renaming. Besides restoring collapsed state, manages
     * special selection after renaming which caused groups merge.
     */
    void onResourceTreeGroupRenamed();

private:

    /**
     * Model and view passed to the constructor are not necessary directly coupled, there are may
     * be arbitrary amount of proxy models in between. This method accept any index from this chain
     * and return lowermost source model index, assuming all data is correct.
     * @param Index from any proxy in chain, i.e drag & drop decorator model, icon decorator model
     *     or filter model. But actually we don't need know implementation details, view model
           index is expected as parameter.
     * @returns Origin model index, source of one passed as parameter. Null QModelIndex in case of
     *     invalid parameter.
     */
    QModelIndex toSourceIndex(const QModelIndex& index) const;

    /**
     * This method serves similar purpose that toSourceIndex, but in reverse. Reliable way to map
     * lowermost source model index to the effective view model index.
     * @param Index of origin model or any proxy model in chain, i.e drag & drop decorator model or
     *     icon decorator model, indexes from deepest source model in the chain are expected.
     * @returns View model index originated from index passed as parameter. Unlike toSourceIndex
     *     method, this one may return null index even if all provided data is valid. For example,
     *     if filtering by proxy model is involved.
     */
    QModelIndex toViewIndex(const QModelIndex& index) const;

    void makeCurrent(const QModelIndex& index);

    void saveViewStateRestoreData(const QnResourceList& resources);
    void clearViewStateRestoreData();

    bool isVisibleByExpandedState(const QModelIndex& sourceIndex) const;
    void expandToMakeVisible(const QModelIndex& sourceIndex) const;
    void restoreExpandedStateFromStoredData();

    QTreeView* treeView() const;
    ui::action::Manager* actionManager() const;

private:
    QnWorkbenchContext* m_workbenchContext;
    QTreeView* m_treeView;
    QAbstractScrollArea* m_scrollArea;
    QAbstractItemModel* m_sourceModel;

    // Resource item indexes affected by a grouping-related action. Stored right before
    // action execution.
    std::vector<QPersistentModelIndex> m_affectedResourceIndexes;

    // Subset of m_affectedResourceIndexes, contains indexes with recursively expanded
    // parents only.
    std::vector<QPersistentModelIndex> m_visibleResourceIndexes;

    // Current item rect, if such is set and have selected state. Stored right before action.
    QRect m_initialCurrentItemRect;

    nx::utils::ScopedConnections m_connections;
};

} // namespace nx::vms::client::desktop
