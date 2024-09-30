// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <unordered_map>

#include <client/client_globals.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/base_notification_observer.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/composition_entity.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/entity_common.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/grouping_rule.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/unique_key_group_list_entity.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/unique_key_list_entity.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_source/abstract_resource_source.h>

namespace nx::vms::client::desktop {
namespace entity_item_model {

//-------------------------------------------------------------------------------------------------
// GroupingEntity declaration.
//-------------------------------------------------------------------------------------------------

/**
 * An entity with similar interface to UniqueKeyListEntity, it's populated via unique key
 * set and Key -> Item transformation function, but this one have 'folding' trait,
 * that means that added items with common attribute, provided by specified role may be
 * grouped together and placed on the deeper level, and this process may be repeated multiple
 * times.
 */
template <class GroupKey, class Key>
class GroupingEntity: public AbstractEntity
{
    using base_type = AbstractEntity;
    using CaseInsensitiveGroupList = UniqueKeyGroupListEntity<GroupKey,
        utils::CaseInsensitiveStringHasher,
        utils::CaseInsensitiveStringEqual>;

public:

    using GroupingRuleStack = std::vector<GroupingRule<GroupKey, Key>>;
    using GroupingRuleConstIterator = typename GroupingRuleStack::const_iterator;

    /**
     * Constructor
     * @param itemCreator Unary operation function with Key type parameter and
     *      AbstractItemPtr return type. It can't be changed during object lifetime.
     * @param keyRole The role which provides Key type value associated to the accessed item.
     * @param itemOrder ItemOrder structure which defines sort order of leaf items.
     * @param groupingRuleStack Sequence of GroupingRule objects which will be applied one-by-one
     *      for each added / modified item to determine item position in hierarchy.
     */
    GroupingEntity(
        const KeyToItemTransform<Key>& itemCreator,
        const int keyRole,
        const ItemOrder& itemOrder,
        const GroupingRuleStack& groupingRuleStack);

    /**
     * Implements AbstractEntity::rowCount().
     */
    virtual int rowCount() const override;

    /**
     * Implements AbstractEntity::childEntity().
     */
    virtual AbstractEntity* childEntity(int row) const override;

    /**
     * Implements AbstractEntity::childEntityRow().
     */
    virtual int childEntityRow(const AbstractEntity* entity) const override;

    /**
     * Implements AbstractEntity()::data().
     */
    virtual QVariant data(int row, int role) const override;

    /**
     * Implements AbstractEntity():flags().
     */
    virtual Qt::ItemFlags flags(int row) const override;

    /**
     * Implements AbstractEntity()::isPlanar().
     * @returns False.
     */
    virtual bool isPlanar() const override;

    /**
     * Sets items resulting from key to item transformation done by provided transform function.
     * Previous contents of the entity will be erased. Each created item will take its place in
     * hierarchy respectively to provided data.
     * @param keys List of keys, duplicates and null keys will be skipped.
     */
    void setItems(const QVector<Key>& keys);

    /**
     * Adds single non null item transformed from the given key.
     * @param key The key.
     * @returns True if it succeeds, i.e item was successfully added.
     */
    bool addItem(const Key& key);

    /**
     * Removes the item described by key if such exists.
     * @param key The key.
     * @returns True if it succeeds, i.e such item was found and removed.
     */
    bool removeItem(const Key& key);

    void installItemSource(const std::shared_ptr<UniqueKeySource<Key>>& keySource);

private:

    std::unique_ptr<CompositionEntity> compositionForStack(
        GroupingRuleConstIterator currentRule,
        int currentRuleOrder);

    UniqueKeyListEntity<Key>* destinationListEntity(const Key& itemKey);

    void updateItemGroup(const Key& itemKey);

    QVector<int> groupDefiningDataRoles();

    void removeEmptyGroups(UniqueKeyListEntity<Key>* tailList);

private:
    std::unique_ptr<CompositionEntity> m_topLevelComposition;

    const KeyToItemTransform<Key> m_itemCreator;
    const int m_keyRole;
    ItemOrder m_itemOrder;
    const GroupingRuleStack m_groupingRuleStack;
    QVector<int> m_groupDefiningDataRoles;

    std::shared_ptr<UniqueKeySource<Key>> m_keySource;

    std::unordered_map<Key, UniqueKeyListEntity<Key>*, Hasher<Key>> m_keyToListMapping;
};

} // namespace entity_item_model {
} // namespace nx::vms::client::desktop
