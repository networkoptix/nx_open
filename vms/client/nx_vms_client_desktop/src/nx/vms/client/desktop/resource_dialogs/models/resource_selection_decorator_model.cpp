// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_selection_decorator_model.h"

#include <numeric>
#include <vector>

#include <client/client_globals.h>
#include <core/resource/resource.h>
#include <nx/vms/client/desktop/common/models/item_model_algorithm.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>

namespace {

QSet<QnResourcePtr> getResources(const QModelIndexList& indexes)
{
    QSet<QnResourcePtr> result;
    for (const auto& index: indexes)
    {
        auto resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
        if (!resource.isNull())
            result.insert(resource);
    }
    return result;
}

} // namespace

namespace nx::vms::client::desktop {

ResourceSelectionDecoratorModel::ResourceSelectionDecoratorModel(
    ResourceSelectionMode selectionMode)
    :
    base_type(),
    m_resourceSelectionMode(selectionMode)
{
    const auto onModelReset =
        [this]
        {
            m_resourceMapping.clear();
            const auto leafIndexes = item_model::getLeafIndexes(this, QModelIndex());
            for (const auto& leafIndex: leafIndexes)
            {
                const auto resource = leafIndex.data(Qn::ResourceRole).value<QnResourcePtr>();
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
                const auto resource = rowIndex.data(Qn::ResourceRole).value<QnResourcePtr>();
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
                const auto resource = rowIndex.data(Qn::ResourceRole).value<QnResourcePtr>();
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
    const auto groupCheckState =
        [this](const QModelIndex& index)
        {
            const auto resources = getResources(item_model::getLeafIndexes(this, index));
            if (m_selectedResources.contains(resources))
                return Qt::Checked;

            if (m_selectedResources.intersects(resources))
                return Qt::PartiallyChecked;

            return Qt::Unchecked;
        };

    const auto leafCheckState =
        [this](const QModelIndex& index) -> QVariant
        {
            const auto resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
            if (resource.isNull())
                return {};

            return m_selectedResources.contains(resource) ? Qt::Checked : Qt::Unchecked;
        };

    if (role == Qt::CheckStateRole)
    {
        if (rowCount(index) == 0)
            return leafCheckState(index);

        switch (m_resourceSelectionMode)
        {
            case ResourceSelectionMode::MultiSelection:
                return groupCheckState(index);

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

Qt::ItemFlags ResourceSelectionDecoratorModel::flags(const QModelIndex& index) const
{
    auto result = base_type::flags(index);
    if (!m_itemsEnabled)
        result.setFlag(Qt::ItemIsEnabled, false);

    return result;
}

QSet<QnResourcePtr> ResourceSelectionDecoratorModel::selectedResources() const
{
    return m_selectedResources;
}

void ResourceSelectionDecoratorModel::setSelectedResources(const QSet<QnResourcePtr>& resources)
{
    m_selectedResources = resources;
    emit dataChanged(index(0, 0), index(rowCount() - 1, 0));
}

QnUuidSet ResourceSelectionDecoratorModel::selectedResourcesIds() const
{
    QnUuidSet result;
    for (const auto& resource : m_selectedResources)
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
                emit dataChanged(parent, parent, {Qt::CheckStateRole});
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
                const auto childResource = leafIndex.data(Qn::ResourceRole).value<QnResourcePtr>();
                if (childResource.isNull())
                    continue;
                childResources.insert(childResource);
            }
            QSet<QPersistentModelIndex> affectedParents;
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
            for (const auto& resource: childResources)
            {
                const auto toggledIndex = m_resourceMapping.value(resource);
                if (toggledIndex.isValid())
                {
                    emit dataChanged(toggledIndex, toggledIndex, {Qt::CheckStateRole});
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
            const auto resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
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
                if (m_resourceSelectionMode != ResourceSelectionMode::MultiSelection)
                {
                    if (!m_selectedResources.empty())
                    {
                        const auto removedIndex = m_resourceMapping.value(*m_selectedResources.begin());
                        if (removedIndex.isValid())
                            emit dataChanged(removedIndex, removedIndex, {Qt::CheckStateRole});
                    }
                    m_selectedResources.clear();
                }
                m_selectedResources.insert(resource);
            }

            invalidateIndexAndParents(index);
            return true;
        };

    if (rowCount(index) > 0)
        return toggleGroupSelection(index);
    return toggleLeafSelection(index);
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

    const auto fillValue = toIndex.data(Qt::CheckStateRole).value<Qt::CheckState>();
    const auto parentIndex = toIndex.parent();

    const auto fillRowsCount = std::abs(toIndex.row() - fromIndex.row());
    const auto firstFillRow = toIndex.row() > fromIndex.row()
        ? fromIndex.row()
        : toIndex.row() + 1;

    for (int row = firstFillRow; row < firstFillRow + fillRowsCount; ++row)
    {
        const auto rowIndex = index(row, 0, parentIndex);
        const auto rowCheckStateData = rowIndex.data(Qt::CheckStateRole);
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

void ResourceSelectionDecoratorModel::setItemsEnabled(bool value)
{
    m_itemsEnabled = value;
}

} // namespace nx::vms::client::desktop
