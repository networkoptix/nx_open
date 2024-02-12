// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "unique_key_group_list_entity.h"

#include <nx/utils/log/assert.h>

#include <nx/vms/client/desktop/resource_views/entity_item_model/entity_model_mapping.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/entity_notification_guard.h>

namespace nx::vms::client::desktop {
namespace entity_item_model {

template <class Key, class KeyHasher, class KeyEqual>
UniqueKeyGroupListEntity<Key, KeyHasher, KeyEqual>::UniqueKeyGroupListEntity(
    const KeyToItemTransform<Key>& headItemCreator,
    const KeyToEntityTransform<Key>& nestedEntityCreator,
    const ItemOrder& itemOrder,
    const QVector<int>& nestedEntityRebuildRoles)
    :
    base_type(),
    m_headItemCreator(headItemCreator),
    m_nestedEntityCreator(nestedEntityCreator),
    m_itemOrder(itemOrder),
    m_nestedEntityRebuildRoles(nestedEntityRebuildRoles)
{
}

template <class Key, class KeyHasher, class KeyEqual>
UniqueKeyGroupListEntity<Key, KeyHasher, KeyEqual>::~UniqueKeyGroupListEntity()
{
    auto guard = removeRowsGuard(modelMapping(), 0, rowCount());
    for (const auto& group: m_groupSequence)
        unassignEntity(group);
    m_groupSequence.clear();
    m_keyMapping.clear();
}

template <class Key, class KeyHasher, class KeyEqual>
int UniqueKeyGroupListEntity<Key, KeyHasher, KeyEqual>::rowCount() const
{
    return static_cast<int>(m_groupSequence.size());
}

template <class Key, class KeyHasher, class KeyEqual>
AbstractEntity* UniqueKeyGroupListEntity<Key, KeyHasher, KeyEqual>::childEntity(int row) const
{
    if (row < 0 || row >= rowCount())
    {
        NX_ASSERT(false, "Invalid row index");
        return nullptr;
    }
    return m_groupSequence[row]->childEntity(0);
}

template <class Key, class KeyHasher, class KeyEqual>
int UniqueKeyGroupListEntity<Key, KeyHasher, KeyEqual>::childEntityRow(const AbstractEntity* entity) const
{
    auto itr = std::find_if(std::begin(m_groupSequence), std::end(m_groupSequence),
        [entity = entity](auto group) { return group->childEntity(0) == entity; });

    if (itr != m_groupSequence.end())
        return std::distance(std::begin(m_groupSequence), itr);

    return -1;
}

template <class Key, class KeyHasher, class KeyEqual>
QVariant UniqueKeyGroupListEntity<Key, KeyHasher, KeyEqual>::data(int row, int role) const
{
    if (row < 0 || row >= rowCount())
    {
        NX_ASSERT(false, "Invalid row index");
        return QVariant();
    }
    return m_groupSequence[row]->data(0, role);
}

template <class Key, class KeyHasher, class KeyEqual>
Qt::ItemFlags UniqueKeyGroupListEntity<Key, KeyHasher, KeyEqual>::flags(int row) const
{
    if (row < 0 || row >= rowCount())
    {
        NX_ASSERT(false, "Invalid row index");
        return Qt::ItemFlags();
    }
    return m_groupSequence[row]->flags(0);
}

template <class Key, class KeyHasher, class KeyEqual>
bool UniqueKeyGroupListEntity<Key, KeyHasher, KeyEqual>::isPlanar() const
{
    return false;
}

template <class Key, class KeyHasher, class KeyEqual>
void UniqueKeyGroupListEntity<Key, KeyHasher, KeyEqual>::setItems(const QVector<Key>& keys)
{
    if (!m_keyMapping.empty())
    {
        auto guard = removeRowsGuard(modelMapping(), 0, rowCount());
        m_keyMapping.clear();
        m_groupSequence.clear();
    }

    if (keys.isEmpty())
        return;

    decltype(m_groupSequence) newGroupSequence;
    newGroupSequence.reserve(keys.size());

    for (const auto& key: keys)
    {
        if (m_keyMapping.find(key) != std::cend(m_keyMapping))
            continue;

        auto keyItr = m_keyMapping.insert(std::make_pair(key, makeGroup(m_headItemCreator(key),
            [this, key]() { return m_nestedEntityCreator(key); }, m_nestedEntityRebuildRoles)));

        GroupEntity* group = keyItr.first->second.get();
        newGroupSequence.push_back(group);
    }

    if (newGroupSequence.empty())
        return;

    std::sort(std::begin(newGroupSequence), std::end(newGroupSequence),
        [this](const auto& lhs, const auto& rhs)
        { return m_itemOrder.comp(lhs->headItem(), rhs->headItem()); });

    auto guard = insertRowsGuard(modelMapping(), 0, newGroupSequence.size());
    m_groupSequence = std::move(newGroupSequence);

    for (auto group: m_groupSequence)
    {
        group->setDataChangedCallback(
            [this](const GroupEntity* group, const QVector<int>& roles)
            { dataChangedHandler(group, roles); });

        assignAsSubEntity(group,
            [this](const AbstractEntity* entity)
            { return groupIndex(static_cast<const GroupEntity*>(entity)); });
    }
}

template <class Key, class KeyHasher, class KeyEqual>
bool UniqueKeyGroupListEntity<Key, KeyHasher, KeyEqual>::addItem(const Key& key)
{
    if (hasItem(key))
        return false;

    auto keyItr = m_keyMapping.insert(std::make_pair(key, makeGroup(m_headItemCreator(key),
        [this, key]() { return m_nestedEntityCreator(key); }, m_nestedEntityRebuildRoles)));
    GroupEntity* group = keyItr.first->second.get();

    auto sequenceItr = std::lower_bound(
        std::begin(m_groupSequence), std::end(m_groupSequence), group,
        [this](const auto& lhs, const auto& rhs)
        { return m_itemOrder.comp(lhs->headItem(), rhs->headItem()); });

    const int index = std::distance(std::begin(m_groupSequence), sequenceItr);

    auto guard = insertRowsGuard(modelMapping(), index);
    m_groupSequence.insert(sequenceItr, group);

    group->setDataChangedCallback(
        [this](const GroupEntity* group, const QVector<int>& roles)
        { dataChangedHandler(group, roles); });

    assignAsSubEntity(group,
        [this](const AbstractEntity* entity)
        { return groupIndex(static_cast<const GroupEntity*>(entity)); });

    return true;
}

template <class Key, class KeyHasher, class KeyEqual>
bool UniqueKeyGroupListEntity<Key, KeyHasher, KeyEqual>::removeItem(const Key& key)
{
    auto keyItr = m_keyMapping.find(key);
    if (keyItr == std::cend(m_keyMapping))
        return false;

    GroupEntity* group = keyItr->second.get();
    auto sequenceItr = std::find(std::cbegin(m_groupSequence), std::cend(m_groupSequence), group);
    const int index = std::distance(std::cbegin(m_groupSequence), sequenceItr);

    auto guard = removeRowsGuard(modelMapping(), index);
    unassignEntity(group);
    m_keyMapping.erase(keyItr);
    m_groupSequence.erase(sequenceItr);

    return true;
}

template <class Key, class KeyHasher, class KeyEqual>
bool UniqueKeyGroupListEntity<Key, KeyHasher, KeyEqual>::hasItem(const Key& key) const
{
    return m_keyMapping.find(key) != std::cend(m_keyMapping);
}

template <class Key, class KeyHasher, class KeyEqual>
int UniqueKeyGroupListEntity<Key, KeyHasher, KeyEqual>::itemIndex(const Key& key) const
{
    auto keyItr = m_keyMapping.find(key);
    if (keyItr == std::cend(m_keyMapping))
        return -1;

    GroupEntity* group = keyItr->second.get();
    return groupIndex(group);
}

template <class Key, class KeyHasher, class KeyEqual>
int UniqueKeyGroupListEntity<Key, KeyHasher, KeyEqual>::groupIndex(const GroupEntity* group) const
{
    auto sequenceItr = std::find(
        std::begin(m_groupSequence), std::end(m_groupSequence), group);

    return std::distance(std::begin(m_groupSequence), sequenceItr);
}

template <class Key, class KeyHasher, class KeyEqual>
void UniqueKeyGroupListEntity<Key, KeyHasher, KeyEqual>::installItemSource(
    const std::shared_ptr<UniqueKeySource<Key>>& keySource)
{
    m_keySource = keySource;
    m_keySource->setKeysHandler = [this](const QVector<Key>& keys) { setItems(keys); };

    using KeyNotifyHandler = typename UniqueKeySource<Key>::KeyNotifyHandler;

    m_keySource->addKeyHandler = std::make_shared<KeyNotifyHandler>(
        [this](const Key& key) { addItem(key); });

    m_keySource->removeKeyHandler = std::make_shared<KeyNotifyHandler>(
        [this](const Key& key) { removeItem(key); });

    m_keySource->initializeRequest();
}

template <class Key, class KeyHasher, class KeyEqual>
void UniqueKeyGroupListEntity<Key, KeyHasher, KeyEqual>::setItemOrder(const ItemOrder& itemOrder)
{
    auto sequentalGenerator = [n = 0]() mutable { return n++; };
    std::vector<std::pair<GroupEntity*, int>> permutationSolver;
    permutationSolver.reserve(m_groupSequence.size());
    for (const auto item: m_groupSequence)
        permutationSolver.emplace_back(item, sequentalGenerator());

    std::sort(std::begin(permutationSolver), std::end(permutationSolver),
        [&itemOrder](const auto& lhs, const auto& rhs)
        { return itemOrder.comp(lhs.first->headItem(), rhs.first->headItem()); });

    if (std::is_sorted(std::cbegin(permutationSolver), std::cend(permutationSolver),
        [&itemOrder](const auto& lhs, const auto& rhs)
        { return itemOrder.comp(lhs.first->headItem(), rhs.first->headItem()); }))
        {
            m_itemOrder = itemOrder;
            return;
        }

    std::vector<int> permutation(permutationSolver.size(), 0);
    std::transform(std::cbegin(permutationSolver), std::cend(permutationSolver),
        std::begin(permutation), [](const auto& itemAndIndex) { return itemAndIndex.second; });

    auto guard = layoutChangeGuard(modelMapping(), permutation);
    m_itemOrder = itemOrder;
    std::transform(std::cbegin(permutationSolver), std::cend(permutationSolver),
        std::begin(m_groupSequence), [](const auto& itemAndIndex)
        { return itemAndIndex.first; });
}

template <class Key, class KeyHasher, class KeyEqual>
void UniqueKeyGroupListEntity<Key, KeyHasher, KeyEqual>::dataChangedHandler(
    const GroupEntity* group,
    const QVector<int> roles)
{
    dataChanged(modelMapping(), roles, groupIndex(group));

    if (m_itemOrder.roles.empty())
        return;

    const auto itemOrderRoles = m_itemOrder.roles;
    const auto matchingRoleItr =
        std::find_first_of(std::cbegin(roles), std::cend(roles),
        std::cbegin(m_itemOrder.roles), std::cend(m_itemOrder.roles));

    if (matchingRoleItr != std::cend(roles))
    {
        auto itemItr =
            std::find(std::begin(m_groupSequence), std::end(m_groupSequence), group);
        int index = std::distance(std::begin(m_groupSequence), itemItr);
        recoverItemSequenceOrder(index);
    }
}

template <class Key, class KeyHasher, class KeyEqual>
int UniqueKeyGroupListEntity<Key, KeyHasher, KeyEqual>::sequenceItemPositionHint(int idx) const
{
    const auto lastIndex = static_cast<int>(m_groupSequence.size()) - 1;

    if (lastIndex < 1)
        return 0;

    if (idx == 0)
    {
        if (!m_itemOrder.comp(m_groupSequence[idx]->headItem(), m_groupSequence[idx + 1]->headItem()))
            return -1;
        return 0;
    }

    if (idx == lastIndex)
    {
        if (!m_itemOrder.comp(m_groupSequence[idx - 1]->headItem(), m_groupSequence[idx]->headItem()))
            return 1;
        return 0;
    }

    if (!m_itemOrder.comp(m_groupSequence[idx - 1]->headItem(), m_groupSequence[idx]->headItem()))
        return 1;

    if (!m_itemOrder.comp(m_groupSequence[idx]->headItem(), m_groupSequence[idx + 1]->headItem()))
        return -1;

    return 0;
}

template <class Key, class KeyHasher, class KeyEqual>
void UniqueKeyGroupListEntity<Key, KeyHasher, KeyEqual>::recoverItemSequenceOrder(int index)
{
    const int positionHint = sequenceItemPositionHint(index);
    if (positionHint == 0)
        return;

    auto groupItr = std::next(std::begin(m_groupSequence), index);
    auto group = m_groupSequence.at(index);

    if (m_groupSequence.size() == 2)
    {
        auto guard = moveRowsGuard(modelMapping(), this, index, 1, this, 1 - positionHint);

        std::iter_swap(groupItr, groupItr - positionHint);
        return;
    }

    if (positionHint < 0)
    {
        auto targetItr = std::lower_bound(
            std::next(groupItr), std::end(m_groupSequence), group,
        [this](const auto& lhs, const auto& rhs)
        { return m_itemOrder.comp(lhs->headItem(), rhs->headItem()); });

        int targetIndex = std::distance(std::begin(m_groupSequence), targetItr);

        auto guard = moveRowsGuard(modelMapping(), this, index, 1, this, targetIndex);
        std::rotate(groupItr, std::next(groupItr), targetItr);
    }
    else
    {
        auto targetItr = std::lower_bound(
            std::begin(m_groupSequence), groupItr, group,
        [this](const auto& lhs, const auto& rhs)
        { return m_itemOrder.comp(lhs->headItem(), rhs->headItem()); });

        const int targetIndex = std::distance(std::begin(m_groupSequence), targetItr);

        auto guard = moveRowsGuard(modelMapping(), this, index, 1, this, targetIndex);
        std::rotate(std::prev(std::make_reverse_iterator(groupItr)),
            std::make_reverse_iterator(groupItr), std::make_reverse_iterator(targetItr));
    }
}

//-------------------------------------------------------------------------------------------------
// UniqueKeyGroupListEntity explicit instantiations.
//-------------------------------------------------------------------------------------------------

template class NX_VMS_CLIENT_DESKTOP_API UniqueKeyGroupListEntity<QnResourcePtr>;
template class NX_VMS_CLIENT_DESKTOP_API UniqueKeyGroupListEntity<nx::Uuid>;
template class NX_VMS_CLIENT_DESKTOP_API UniqueKeyGroupListEntity<QString>;
template class NX_VMS_CLIENT_DESKTOP_API UniqueKeyGroupListEntity<QString, CaseInsensitiveStringHasher, CaseInsensitiveStringEqual>;
template class NX_VMS_CLIENT_DESKTOP_API UniqueKeyGroupListEntity<int>;

} // namespace entity_item_model
} // namespace nx::vms::client::desktop
