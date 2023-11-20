// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "grouping_entity.h"

#include <nx/vms/client/desktop/resource_views/entity_item_model/entity_model_mapping.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/entity_notification_guard.h>

namespace {

using namespace nx::vms::client::desktop::entity_item_model;

template <typename T>
T* getNearestParentEntityWithType(
    const AbstractEntity* originEntity,
    const AbstractEntity* lowerBoundEntity)
{
    if (!originEntity)
        return nullptr;

    auto parent = originEntity->parentEntity();
    while (parent != nullptr)
    {
        if (auto result = dynamic_cast<T*>(parent))
            return result;

        if (parent == lowerBoundEntity)
            break;

        parent = parent->parentEntity();
    }
    return nullptr;
};

} // namespace

namespace nx::vms::client::desktop {
namespace entity_item_model {

//-------------------------------------------------------------------------------------------------
// GroupDefiningDataChangeObserver definition.
//-------------------------------------------------------------------------------------------------

template <class Key>
GroupDefiningDataChangeObserver<Key>::GroupDefiningDataChangeObserver(
    AbstractEntity* observedEntity,
    int keyRole,
    const QVector<int>& observedGroupDefiningRoles,
    const GroupDefiningDataChangeCallback& groupDefiningDataChangeCallback)
    :
    m_observedEntity(observedEntity),
    m_keyRole(keyRole),
    m_observedGroupDefiningRoles(observedGroupDefiningRoles),
    m_groupDefiningDataChangeCallback(groupDefiningDataChangeCallback)
{
}

template <class Key>
void GroupDefiningDataChangeObserver<Key>::dataChanged(int first, int last,
    const QVector<int>& roles)
{
    if (roles.isEmpty() || std::cend(m_observedGroupDefiningRoles) != std::find_first_of(
        std::cbegin(m_observedGroupDefiningRoles), std::cend(m_observedGroupDefiningRoles),
        std::cbegin(roles), std::cend(roles)))
    {
        if (first == last)
        {
            m_groupDefiningDataChangeCallback(
                m_observedEntity->data(first, m_keyRole).template value<Key>());
            return;
        }

        std::vector<Key> keysToUpdate;
        for (int row = first; row <= last; ++row)
            keysToUpdate.push_back(m_observedEntity->data(row, m_keyRole).template value<Key>());

        for (const auto& key: keysToUpdate)
            m_groupDefiningDataChangeCallback(key);
    }
}

//-------------------------------------------------------------------------------------------------
// GroupingEntity definition.
//-------------------------------------------------------------------------------------------------

template <class GroupKey, class Key>
GroupingEntity<GroupKey, Key>::GroupingEntity(
    const KeyToItemTransform<Key>& itemCreator,
    const int keyRole,
    const ItemOrder& itemOrder,
    const GroupingRuleStack& groupingRuleStack)
    :
    base_type(),
    m_itemCreator(itemCreator),
    m_keyRole(keyRole),
    m_itemOrder(itemOrder),
    m_groupingRuleStack(groupingRuleStack)
{
    m_groupDefiningDataRoles = groupDefiningDataRoles();

    m_topLevelComposition =
        compositionForStack(std::cbegin(m_groupingRuleStack), /*stackBeginAtOrder*/ 0);

    assignAsSubEntity(m_topLevelComposition.get(), [](const AbstractEntity*) { return 0; });
}

template <class GroupKey, class Key>
int GroupingEntity<GroupKey, Key>::rowCount() const
{
    return m_topLevelComposition->rowCount();
}

template <class GroupKey, class Key>
AbstractEntity* GroupingEntity<GroupKey, Key>::childEntity(int row) const
{
    return m_topLevelComposition->childEntity(row);
}

template <class GroupKey, class Key>
int GroupingEntity<GroupKey, Key>::childEntityRow(const AbstractEntity* entity) const
{
    return m_topLevelComposition->childEntityRow(entity);
}

template <class GroupKey, class Key>
QVariant GroupingEntity<GroupKey, Key>::data(int row, int role) const
{
    return m_topLevelComposition->data(row, role);
}

template <class GroupKey, class Key>
Qt::ItemFlags GroupingEntity<GroupKey, Key>::flags(int row) const
{
    return m_topLevelComposition->flags(row);
}

template <class GroupKey, class Key>
bool GroupingEntity<GroupKey, Key>::isPlanar() const
{
    return false;
}

template <class GroupKey, class Key>
void GroupingEntity<GroupKey, Key>::setItems(const QVector<Key>& keys)
{
    {
        auto guard = removeRowsGuard(modelMapping(), 0, rowCount());
        m_keyToListMapping.clear();
        m_topLevelComposition = compositionForStack(
            std::cbegin(m_groupingRuleStack), /*stackBeginAtOrder*/ 0);
        assignAsSubEntity(m_topLevelComposition.get(), [](const AbstractEntity*) { return 0; });
    }

    for (const auto& key: keys)
        addItem(key);
}

template <class GroupKey, class Key>
bool GroupingEntity<GroupKey, Key>::addItem(const Key& key)
{
    auto destinationList = destinationListEntity(key);
    m_keyToListMapping.insert(std::make_pair(key, destinationList));

    return destinationList->addItem(key);
}

template <class GroupKey, class Key>
bool GroupingEntity<GroupKey, Key>::removeItem(const Key& key)
{
    auto itr = m_keyToListMapping.find(key);
    if (itr == m_keyToListMapping.cend())
        return false;

    auto listEntity = itr->second;
    m_keyToListMapping.erase(itr);
    const auto result = listEntity->removeItem(key);

    removeEmptyGroups(listEntity);

    return result;
}

template <class GroupKey, class Key>
void GroupingEntity<GroupKey, Key>::installItemSource(
    const std::shared_ptr<UniqueKeySource<Key>>& keySource)
{
    m_keySource = keySource;
    m_keySource->setKeysHandler =
        [this](const QVector<Key>& keys) { setItems(keys); };

    m_keySource->addKeyHandler =
        std::make_shared<typename UniqueKeySource<Key>::KeyNotifyHandler>(
        [this](const Key& key) { addItem(key); });

    m_keySource->removeKeyHandler =
        std::make_shared<typename UniqueKeySource<Key>::KeyNotifyHandler>(
        [this](const Key& key) { removeItem(key); });

    m_keySource->initializeRequest();
}

template <class GroupKey, class Key>
UniqueKeyListEntity<Key>* GroupingEntity<GroupKey, Key>::destinationListEntity(const Key& itemKey)
{
    const auto stackBegin = std::cbegin(m_groupingRuleStack);
    const auto stackEnd = std::cend(m_groupingRuleStack);

    // Rule which corresponds group list entity at index 0 of current composition.
    auto originRuleItr = stackBegin;
    CompositionEntity* currentComposition = m_topLevelComposition.get();

    for (auto ruleItr = stackBegin; ruleItr != stackEnd; std::advance(ruleItr, 1))
    {
        for (int ruleOrder = 0; ruleOrder < ruleItr->dimension; ++ruleOrder)
        {
            auto groupId = ruleItr->itemClassifier(itemKey, ruleOrder);
            if (isNull(groupId))
                break;

            auto groupSubEntity =
                currentComposition->subEntityAt(std::distance(originRuleItr, ruleItr));
            auto groupList = static_cast<CaseInsensitiveGroupList*>(groupSubEntity);

            if (!groupList->hasItem(groupId))
                groupList->addItem(groupId);

            currentComposition = static_cast<CompositionEntity*>(
                groupList->childEntity(groupList->itemIndex(groupId)));

            if (ruleOrder + 1 == ruleItr->dimension)
                originRuleItr = std::next(ruleItr);
        }
    }

    return static_cast<UniqueKeyListEntity<Key>*>(currentComposition->lastSubentity());
}

template <class GroupKey, class Key>
std::unique_ptr<CompositionEntity>GroupingEntity<GroupKey, Key>::compositionForStack(
    GroupingRuleConstIterator currentRuleItr,
    int currentRuleOrder)
{
    auto result = std::make_unique<CompositionEntity>();

    for (auto ruleItr = currentRuleItr; ruleItr != std::cend(m_groupingRuleStack); ++ruleItr)
    {
        const bool isLastOrderForRule = (currentRuleOrder == ruleItr->dimension - 1);
        const auto nextRule = isLastOrderForRule ? std::next(ruleItr) : ruleItr;
        const auto nextRuleOrder = isLastOrderForRule ? 0 : (currentRuleOrder + 1);

        const auto nestedEntityCreator =
            [this, nextRule, nextRuleOrder] (const auto& /*groupKey*/)
            {
                return compositionForStack(nextRule, nextRuleOrder);
            };

        result->addSubEntity(makeUniqueKeyGroupList<
            GroupKey, CaseInsensitiveStringHasher, CaseInsensitiveStringEqual>(
                ruleItr->groupKeyToItemTransform,
                nestedEntityCreator,
                ruleItr->groupItemOrder));
    }

    auto listEntity = makeKeyList<Key>(m_itemCreator, m_itemOrder);

    auto groupingDataChangeObserver = std::make_unique<GroupDefiningDataChangeObserver<Key>>(
        listEntity.get(),
        m_keyRole,
        m_groupDefiningDataRoles,
        [this](const Key& key) { updateItemGroup(key); });

    listEntity->setNotificationObserver(std::move(groupingDataChangeObserver));

    result->addSubEntity(std::move(listEntity));

    return result;
}

template <class GroupKey, class Key>
QVector<int> GroupingEntity<GroupKey, Key>::groupDefiningDataRoles()
{
    QVector<int> result;
    for (const auto& rule: m_groupingRuleStack)
    {
        const auto role = rule.groupKeyRole;
        if (std::find(std::cbegin(result), std::cend(result), role) == std::cend(result))
            result.push_back(role);
    }
    return result;
}

template <class GroupKey, class Key>
void GroupingEntity<GroupKey, Key>::removeEmptyGroups(UniqueKeyListEntity<Key>* tailList)
{
    if (tailList->rowCount() > 0)
        return;

    const auto boundEntity = m_topLevelComposition.get();

    auto parentGroup = getNearestParentEntityWithType<GroupEntity>(tailList, boundEntity);
    auto parentGroupList = getNearestParentEntityWithType<CaseInsensitiveGroupList>(
        parentGroup, boundEntity);

    while (parentGroupList)
    {
        if (parentGroup->childRowCount(0) != 0)
            return;

        bool removed = false;
        for (const auto& rule: m_groupingRuleStack)
        {
            const auto ruleGroupDefiningData = parentGroup->data(0, rule.groupKeyRole);
            if (isNull(ruleGroupDefiningData.template value<GroupKey>()))
                continue;

            removed = parentGroupList->removeItem(ruleGroupDefiningData.template value<GroupKey>());
            break;
        }
        if (!removed)
            return;

        parentGroup = getNearestParentEntityWithType<GroupEntity>(parentGroupList, boundEntity);
        parentGroupList = getNearestParentEntityWithType<CaseInsensitiveGroupList>(
            parentGroup, boundEntity);
    }
}

template <class GroupKey, class Key>
void GroupingEntity<GroupKey, Key>::updateItemGroup(const Key& itemKey)
{
    auto itr = m_keyToListMapping.find(itemKey);
    if (itr == m_keyToListMapping.cend())
    {
        NX_ASSERT("Missing reference to the list containing updating item");
        return;
    }

    auto sourceList = itr->second;
    auto destinationList = destinationListEntity(itemKey);

    if (sourceList == destinationList)
        return;

    sourceList->moveItem(itemKey, destinationList);
    m_keyToListMapping.insert_or_assign(itr, itemKey, destinationList);

    removeEmptyGroups(sourceList);
}

//-------------------------------------------------------------------------------------------------
// Explicit instantiations.
//-------------------------------------------------------------------------------------------------

template class NX_VMS_CLIENT_DESKTOP_API GroupDefiningDataChangeObserver<QnResourcePtr>;
template class NX_VMS_CLIENT_DESKTOP_API GroupDefiningDataChangeObserver<int>;
template class NX_VMS_CLIENT_DESKTOP_API GroupDefiningDataChangeObserver<QnUuid>;

template class NX_VMS_CLIENT_DESKTOP_API GroupingEntity<QString, QnResourcePtr>;
template class NX_VMS_CLIENT_DESKTOP_API GroupingEntity<QString, int>;
template class NX_VMS_CLIENT_DESKTOP_API GroupingEntity<QString, QnUuid>;

} // namespace entity_item_model
} // namespace nx::vms::client::desktop
