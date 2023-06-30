// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_selection_decorator_model.h"

#include <client/client_globals.h>
#include <core/resource/resource.h>
#include <nx/vms/client/desktop/common/models/item_model_algorithm.h>
#include <nx/vms/client/desktop/resource_dialogs/resource_dialogs_constants.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>

namespace {

static constexpr int kResourceColumn = 0;

int checkBoxColumn(const QAbstractItemModel* model)
{
    return model->columnCount() - 1;
}

QSet<QnResourcePtr> getResources(const QModelIndexList& indexes)
{
    QSet<QnResourcePtr> result;
    for (const auto& index: indexes)
    {
        auto resource = index.data(nx::vms::client::core::ResourceRole).value<QnResourcePtr>();
        if (!resource.isNull())
            result.insert(resource);
    }
    return result;
}

} // namespace

namespace nx::vms::client::desktop {

ResourceSelectionDecoratorModel::ResourceSelectionDecoratorModel(
    ResourceSelectionMode selectionMode, QObject* parent)
    :
    base_type(parent),
    m_resourceSelectionMode(selectionMode)
{
    const auto onModelReset =
        [this]
        {
            m_resourceMapping.clear();
            const auto leafIndexes = item_model::getLeafIndexes(this, QModelIndex());
            for (const auto& leafIndex: leafIndexes)
            {
                const auto resource = leafIndex.data(core::ResourceRole).value<QnResourcePtr>();
                if (!resource.isNull())
                    m_resourceMapping.insert(resource, leafIndex);
            }
        };

    const auto onRowsInserted =
        [this](const QModelIndex& parent, int first, int last)
        {
            for (int row = first; row <= last; ++row)
            {
                const auto rowIndex = index(row, 0, parent);
                const auto resource = rowIndex.data(core::ResourceRole).value<QnResourcePtr>();
                if (!resource.isNull())
                    m_resourceMapping.insert(resource, rowIndex);
            }
        };

    const auto beforeRowsRemoved =
        [this](const QModelIndex& parent, int first, int last)
        {
            for (int row = first; row <= last; ++row)
            {
                const auto rowIndex = index(row, 0, parent);
                const auto resource = rowIndex.data(core::ResourceRole).value<QnResourcePtr>();
                if (!resource.isNull())
                    m_resourceMapping.remove(resource);
            }
        };

    connect(this, &ResourceSelectionDecoratorModel::modelReset, onModelReset);
    connect(this, &ResourceSelectionDecoratorModel::rowsInserted, onRowsInserted);
    connect(this, &ResourceSelectionDecoratorModel::rowsAboutToBeRemoved, beforeRowsRemoved);
}

QVariant ResourceSelectionDecoratorModel::data(const QModelIndex& index, int role) const
{
    const auto resourceIndex = index.siblingAtColumn(kResourceColumn);

    const auto groupCheckState =
        [this](const QModelIndex& resourceIndex)
        {
            const auto resources = getResources(item_model::getLeafIndexes(this, resourceIndex));
            if (m_selectedResources.contains(resources))
                return Qt::Checked;

            if (m_selectedResources.intersects(resources))
                return Qt::PartiallyChecked;

            return Qt::Unchecked;
        };

    const auto leafCheckState =
        [this](const QModelIndex& index) -> QVariant
        {
            const auto resource = index.data(core::ResourceRole).value<QnResourcePtr>();
            if (resource.isNull())
                return {};

            return m_selectedResources.contains(resource) ? Qt::Checked : Qt::Unchecked;
        };

    if (role == Qt::CheckStateRole && index.column() == checkBoxColumn(this))
    {
        if (rowCount(resourceIndex) == 0)
            return leafCheckState(resourceIndex);

        switch (m_resourceSelectionMode)
        {
            case ResourceSelectionMode::MultiSelection:
                return groupCheckState(resourceIndex);

            case ResourceSelectionMode::SingleSelection:
            case ResourceSelectionMode::ExclusiveSelection:
                return QVariant();

            default:
                NX_ASSERT(false, "Resource selection model: unexpected selection mode");
                return QVariant();
        }
    }
    if (role == ResourceDialogItemRole::IsItemHighlightedRole && index.column() == 0)
    {
        if (rowCount(resourceIndex) == 0)
            return leafCheckState(resourceIndex).value<Qt::CheckState>() != Qt::Unchecked;

        switch (m_resourceSelectionMode)
        {
            case ResourceSelectionMode::MultiSelection:
                return groupCheckState(resourceIndex) != Qt::Unchecked;

            case ResourceSelectionMode::SingleSelection:
            case ResourceSelectionMode::ExclusiveSelection:
                return QVariant();

            default:
                NX_ASSERT(false, "Resource selection model: unexpected selection mode");
                return QVariant();
        }
    }

    return base_type::data(index, role);
}

QHash<int, QByteArray> ResourceSelectionDecoratorModel::roleNames() const
{
    auto roleNames = base_type::roleNames();
    roleNames.insert(Qt::CheckStateRole, "checkState");
    return roleNames;
}

QSet<QnResourcePtr> ResourceSelectionDecoratorModel::selectedResources() const
{
    return m_selectedResources;
}

void ResourceSelectionDecoratorModel::setSelectedResources(const QSet<QnResourcePtr>& resources)
{
    m_selectedResources = resources;
    emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
    emit selectedResourcesChanged();
}

QnUuidSet ResourceSelectionDecoratorModel::selectedResourcesIds() const
{
    QnUuidSet result;
    for (const auto& resource: m_selectedResources)
        result.insert(resource->getId());
    return result;
}

bool ResourceSelectionDecoratorModel::toggleSelection(const QModelIndex& index)
{
    if (!index.isValid() || index.model() != this)
        return NX_ASSERT(false, "Invalid index");

    const auto invalidateIndexAndParents =
        [this](const QModelIndex& index)
        {
            QModelIndex parent = index;
            while (parent.isValid())
            {
                emit dataChanged(
                    parent.siblingAtColumn(kResourceColumn),
                    parent.siblingAtColumn(checkBoxColumn(this)),
                    {Qt::CheckStateRole});
                parent = parent.parent();
            }
        };

    const auto toggleGroupSelection =
        [this, &invalidateIndexAndParents](const QModelIndex& index)
        {
            if (m_resourceSelectionMode != ResourceSelectionMode::MultiSelection)
                return false;

            QSet<QnResourcePtr> childResources;
            const auto leafIndexes = item_model::getLeafIndexes(this, index);
            for (const auto& leafIndex: leafIndexes)
            {
                const auto childResource = leafIndex.data(core::ResourceRole).value<QnResourcePtr>();
                if (childResource.isNull())
                    continue;
                childResources.insert(childResource);
            }

            if (m_selectedResources.contains(childResources))
            {
                childResources.intersect(m_selectedResources);
                m_selectedResources.subtract(childResources);
            }
            else
            {
                childResources.subtract(m_selectedResources);
                m_selectedResources.unite(childResources);
            }

            emit selectedResourcesChanged();

            QSet<QPersistentModelIndex> affectedParents;
            for (const auto& resource: childResources)
            {
                const QModelIndex toggledIndex = resourceIndex(resource);
                if (toggledIndex.isValid())
                {
                    emit dataChanged(
                        toggledIndex.siblingAtColumn(kResourceColumn),
                        toggledIndex.siblingAtColumn(checkBoxColumn(this)),
                        {Qt::CheckStateRole});
                    affectedParents.insert(toggledIndex.parent());
                }
            }

            for (const auto& affectedParent: affectedParents)
                invalidateIndexAndParents(affectedParent);
            if (childResources.size() > 0)
                invalidateIndexAndParents(index);

            return true;
        };

    const auto toggleLeafSelection =
        [this, &invalidateIndexAndParents](const QModelIndex& index)
        {
            const auto resource = index.data(core::ResourceRole).value<QnResourcePtr>();
            if (resource.isNull())
                return false;

            if (m_selectedResources.contains(resource))
            {
                if (m_resourceSelectionMode == ResourceSelectionMode::ExclusiveSelection)
                    return false;

                m_selectedResources.remove(resource);
            }
            else
            {
                QModelIndex removedIndex;
                if (m_resourceSelectionMode != ResourceSelectionMode::MultiSelection)
                {
                    if (!m_selectedResources.empty())
                        removedIndex = resourceIndex(*m_selectedResources.begin());

                    m_selectedResources.clear();
                }

                m_selectedResources.insert(resource);
                emit selectedResourcesChanged();

                if (removedIndex.isValid())
                {
                    emit dataChanged(
                        removedIndex.siblingAtColumn(0),
                        removedIndex.siblingAtColumn(columnCount(removedIndex.parent()) - 1),
                        {Qt::CheckStateRole});
                }
            }

            invalidateIndexAndParents(index);
            return true;
        };

    return rowCount(index) > 0
        ? toggleGroupSelection(index)
        : toggleLeafSelection(index);
}

bool ResourceSelectionDecoratorModel::toggleSelection(
    const QModelIndex& fromIndex,
    const QModelIndex& toIndex)
{
    if (!toIndex.isValid() || toIndex.model() != this)
        return NX_ASSERT(false, "Invalid toIndex");

    if (fromIndex.isValid() && fromIndex.model() != this)
        return NX_ASSERT(false, "Invalid fromIndex");

    if (!fromIndex.isValid()
        || fromIndex.parent() != toIndex.parent()
        || fromIndex == toIndex
        || m_resourceSelectionMode != ResourceSelectionMode::MultiSelection)
    {
        return toggleSelection(toIndex);
    }

    if (!toggleSelection(toIndex))
        return false;

    const auto fillValue = toIndex.siblingAtColumn(checkBoxColumn(this))
        .data(Qt::CheckStateRole).value<Qt::CheckState>();
    const auto parentIndex = toIndex.parent();

    const auto fillRowsCount = std::abs(toIndex.row() - fromIndex.row());
    const auto firstFillRow = toIndex.row() > fromIndex.row()
        ? fromIndex.row()
        : toIndex.row() + 1;

    for (int row = firstFillRow; row < firstFillRow + fillRowsCount; ++row)
    {
        const auto rowIndex = index(row, toIndex.column(), parentIndex);
        const auto rowCheckStateData = rowIndex.siblingAtColumn(checkBoxColumn(this))
            .data(Qt::CheckStateRole);
        if (rowCheckStateData.isNull())
            continue;

        const auto rowCheckState = rowCheckStateData.value<Qt::CheckState>();
        if (rowCheckState == fillValue)
            continue;

        toggleSelection(rowIndex);
    }

    return true;
}

ResourceSelectionMode ResourceSelectionDecoratorModel::selectionMode() const
{
    return m_resourceSelectionMode;
}

void ResourceSelectionDecoratorModel::setSelectionMode(ResourceSelectionMode mode)
{
    if (m_resourceSelectionMode == mode)
        return;

    m_resourceSelectionMode = mode;
}

QModelIndex ResourceSelectionDecoratorModel::resourceIndex(const QnResourcePtr& resource) const
{
    return m_resourceMapping.value(resource);
}

} // namespace nx::vms::client::desktop
