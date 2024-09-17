// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <unordered_map>
#include <vector>

#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/abstract_entity.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/entity_common.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/group_entity.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/unique_key_source.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/item/abstract_item.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/item_order/item_order.h>

namespace nx::vms::client::desktop {
namespace entity_item_model {

/**
* @todo Add documentation.
*/
template <
    class Key,
    class KeyHasher = Hasher<Key>,
    class KeyEqual = std::equal_to<Key>>
class UniqueKeyGroupListEntity: public AbstractEntity
{
    using base_type = AbstractEntity;

public:
    /**
     * @todo Add documentation.
     */
    UniqueKeyGroupListEntity(
        const KeyToItemTransform<Key>& headItemCreator,
        const KeyToEntityTransform<Key>& nestedEntityCreator,
        const ItemOrder& itemOrder,
        const QVector<int>& nestedEntityRebuildRoles = {});

    /**
     * @todo Add documentation.
     */
    virtual ~UniqueKeyGroupListEntity() override;

    /**
     * @todo Add documentation.
     */
    virtual int rowCount() const override;

    /**
     * @todo Add documentation.
     */
    virtual AbstractEntity* childEntity(int row) const override;

    /**
     * @todo Add documentation.
     */
    virtual int childEntityRow(const AbstractEntity* entity) const override;

    /**
     * @todo Add documentation.
     */
    virtual QVariant data(int row, int role = Qt::DisplayRole) const override;

    /**
     * @todo Add documentation.
     */
    virtual Qt::ItemFlags flags(int row) const override;

    /**
     * @todo Add documentation.
     */
    virtual bool isPlanar() const override;

    /**
     * @todo Add documentation.
     */
    void setItems(const QVector<Key>& keys);

    /**
     * @todo Add documentation.
     */
    bool addItem(const Key& key);

    /**
     * Moves group entity corresponding to the given key to the another group list if such
     * operation is performable i.e this list contains such group entity while other list is
     * not. Generates 'move' notifications, persistent indexes of items from moved group won't be
     * invalidated.
     * @note Key -> Group transformation of other group list will be bypassed. Consistency of the
     *     resulting contents is the responsibility of the caller.
     * @returns True if operation succeeds, i.e group entity was successfully moved.
     */
    bool moveItem(const Key& key, UniqueKeyGroupListEntity<Key, KeyHasher, KeyEqual>* otherList);

    /**
     * @todo Add documentation.
     */
    bool removeItem(const Key& key);

    /**
     * @todo Add documentation.
     */
    bool hasItem(const Key& key) const;

    /**
     * @todo Add documentation.
     */
    int itemIndex(const Key& key) const;

    /**
     * @todo Add documentation.
     */
    int groupIndex(const GroupEntity* group) const;

    /**
     * @todo Add documentation.
     */
    void installItemSource(const std::shared_ptr<UniqueKeySource<Key>>& keySource);

    /**
     * @todo Add documentation.
     */
    void setItemOrder(const ItemOrder& itemOrder);

protected:

    /**
     * @todo Add documentation.
     */
    void dataChangedHandler(const GroupEntity* group, const QVector<int> roles);

    /**
     * @todo Add documentation.
     */
    int sequenceItemPositionHint(int idx) const;

    /**
     * @todo Add documentation.
     */
    void recoverItemSequenceOrder(int index);

private:
    std::shared_ptr<UniqueKeySource<Key>> m_keySource;
    KeyToItemTransform<Key> m_headItemCreator;
    KeyToEntityTransform<Key> m_nestedEntityCreator;
    ItemOrder m_itemOrder;
    QVector<int> m_nestedEntityRebuildRoles;

    std::unordered_map<Key, GroupEntityPtr, KeyHasher, KeyEqual> m_keyMapping;
    std::vector<GroupEntity*> m_groupSequence;
};

/**
 * Convenient non-member factory function.
 */
template <
    class Key,
    class KeyHasher = Hasher<Key>,
    class KeyEqual = std::equal_to<Key>>
std::unique_ptr<UniqueKeyGroupListEntity<Key, KeyHasher, KeyEqual>> makeUniqueKeyGroupList(
    const KeyToItemTransform<Key>& headItemCreator,
    const KeyToEntityTransform<Key>& nestedEntityCreator,
    const ItemOrder& itemOrder,
    const QVector<int>& nestedEntityRebuildRoles = {})
{
    return std::make_unique<UniqueKeyGroupListEntity<Key, KeyHasher, KeyEqual>>
        (headItemCreator, nestedEntityCreator, itemOrder, nestedEntityRebuildRoles);
}

} // namespace entity_item_model
} // namespace nx::vms::client::desktop
