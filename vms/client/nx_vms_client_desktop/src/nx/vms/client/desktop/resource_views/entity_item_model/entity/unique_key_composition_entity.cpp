// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "unique_key_composition_entity.h"

#include <nx/vms/client/desktop/resource_views/entity_item_model/entity_model_mapping.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/entity_notification_guard.h>

namespace nx::vms::client::desktop {
namespace entity_item_model {

template <class Key>
UniqueKeyCompositionEntity<Key>::UniqueKeyCompositionEntity():
    base_type()
{
}

template <class Key>
UniqueKeyCompositionEntity<Key>::~UniqueKeyCompositionEntity()
{
    resetNotificationObserver();
    auto guard = removeRowsGuard(modelMapping(), 0, rowCount());
    for (const auto& keyEntityPair : m_subEntities)
        unassignEntity(keyEntityPair.entity.get());
    m_subEntities.clear();
}

template <class Key>
int UniqueKeyCompositionEntity<Key>::rowCount() const
{
    return std::accumulate(std::cbegin(m_subEntities), std::cend(m_subEntities), 0,
        [](int sum, const auto& subEntity) { return sum + subEntity.entity->rowCount(); });
}

template <class Key>
AbstractEntity* UniqueKeyCompositionEntity<Key>::childEntity(int row) const
{
    const auto subEntityWithRow = subEntityWithLocalRow(row);
    if (subEntityWithRow.first == nullptr)
    {
        NX_ASSERT(false, "Row out of bounds");
        return nullptr;
    }
    return subEntityWithRow.first->childEntity(subEntityWithRow.second);
}

template <class Key>
int UniqueKeyCompositionEntity<Key>::childEntityRow(const AbstractEntity* entity) const
{
    int offset = 0;
    for (const auto& keyEntityPair: m_subEntities)
    {
        if (!keyEntityPair.entity->isPlanar())
        {
            const auto subEntityChildEntityRow = keyEntityPair.entity->childEntityRow(entity);
            if (subEntityChildEntityRow != -1)
                return subEntityChildEntityRow + offset;
        }
        offset += keyEntityPair.entity->rowCount();
    }
    return -1;
}

template <class Key>
QVariant UniqueKeyCompositionEntity<Key>::data(int row, int role /*= Qt::DisplayRole*/) const
{
    const auto subEntityWithRow = subEntityWithLocalRow(row);
    if (subEntityWithRow.first == nullptr)
    {
        NX_ASSERT(false, "Row out of bounds");
        return {};
    }
    auto result = subEntityWithRow.first->data(subEntityWithRow.second, role);
    return result;
}

template <class Key>
Qt::ItemFlags UniqueKeyCompositionEntity<Key>::flags(int row) const
{
    const auto subEntityWithRow = subEntityWithLocalRow(row);
    if (subEntityWithRow.first == nullptr)
    {
        NX_ASSERT(false, "Row out of bounds");
        return {};
    }
    return subEntityWithRow.first->flags(subEntityWithRow.second);
}

template <class Key>
AbstractEntity* UniqueKeyCompositionEntity<Key>::subEntity(const Key& key)
{
    auto itr = std::lower_bound(std::cbegin(m_subEntities), std::cend(m_subEntities),
        KeyEntityPair(key));
    if (itr != std::cend(m_subEntities) && itr->key == key)
        return itr->entity.get();

    return nullptr;
}

template <class Key>
bool UniqueKeyCompositionEntity<Key>::isPlanar() const
{
    return false;
}

template <class Key>
void UniqueKeyCompositionEntity<Key>::setSubEntity(const Key& key, AbstractEntityPtr subEntityPtr)
{
    auto itr = std::lower_bound(std::cbegin(m_subEntities), std::cend(m_subEntities),
        KeyEntityPair(key));

    if (itr != std::cend(m_subEntities) && itr->key == key)
    {
        if (itr->entity == subEntityPtr)
            return;
        else
            removeSubEntity(key);
    }

    const auto subEntity = subEntityPtr.get();
    const auto subEntityRowCount = subEntityPtr->rowCount();
    if (m_subEntities.empty())
    {
        auto guard = insertRowsGuard(modelMapping(), 0, subEntityRowCount);
        m_subEntities.push_back({key, std::move(subEntityPtr)});
        assignAsSubEntity(subEntity,
            [this](const AbstractEntity* entity) { return subEntityOffset(entity); });
    }
    else
    {
        itr = std::lower_bound(std::cbegin(m_subEntities), std::cend(m_subEntities),
            KeyEntityPair(key));

        const auto offset = (itr == std::cend(m_subEntities))
            ? rowCount()
            : subEntityOffset(itr->entity.get());

        auto guard = insertRowsGuard(modelMapping(), offset, subEntityRowCount);

        m_subEntities.insert(itr, {key, std::move(subEntityPtr)});
        assignAsSubEntity(subEntity,
            [this](const AbstractEntity* entity) { return subEntityOffset(entity); });
    }
}

template <class Key>
bool UniqueKeyCompositionEntity<Key>::removeSubEntity(const Key& key)
{
    auto index = subEntityIndex(key);
    if (index == -1)
        return false;

    const auto subEntity = m_subEntities.at(index).entity.get();

    auto guard = removeRowsGuard(
        modelMapping(), subEntityOffset(subEntity), subEntity->rowCount());

    unassignEntity(subEntity);
    m_subEntities.erase(std::next(m_subEntities.begin(), index));

    return true;
}

template <class Key>
int UniqueKeyCompositionEntity<Key>::subEntityIndex(const Key& key) const
{
    auto itr = std::lower_bound(std::cbegin(m_subEntities), std::cend(m_subEntities),
        KeyEntityPair(key));

    if (itr == std::cend(m_subEntities) || itr->key != key)
        return -1;

    return std::distance(std::cbegin(m_subEntities), itr);
}

template <class Key>
int UniqueKeyCompositionEntity<Key>::subEntityOffset(const AbstractEntity* subEntity) const
{
    int result = 0;
    for (const auto& keyEntityPair: m_subEntities)
    {
        if (keyEntityPair.entity.get() == subEntity)
            return result;
        result += keyEntityPair.entity->rowCount();
    }
    NX_ASSERT(false, "Invalid argument");
    return result;
}

template <class Key>
std::pair<AbstractEntity*, int> UniqueKeyCompositionEntity<Key>::subEntityWithLocalRow(int row) const
{
    if (row < 0)
    {
        NX_ASSERT(false, "Row index cannot be negative");
        return {nullptr, -1};
    }
    int subentityRow = row;
    for (const auto& keyEntityPair: m_subEntities)
    {
        const auto subEntityRowCount = keyEntityPair.entity->rowCount();
        if (subEntityRowCount > subentityRow)
            return std::make_pair(keyEntityPair.entity.get(), subentityRow);
        else
            subentityRow -= subEntityRowCount;
    }
    return {nullptr, -1};
}

//-------------------------------------------------------------------------------------------------
// UniqueKeyCompositionEntity explicit instantiations.
//-------------------------------------------------------------------------------------------------

template class NX_VMS_CLIENT_DESKTOP_API UniqueKeyCompositionEntity<int>;

} // namespace entity_item_model
} // namespace nx::vms::client::desktop
