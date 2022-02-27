// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>
#include <unordered_map>

#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/abstract_entity.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/item/abstract_item.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/item_order/item_order.h>

#include "unique_key_source.h"

namespace nx::vms::client::desktop {
namespace entity_item_model {

template <typename Key, typename = std::enable_if_t<std::is_object<Key>::value>>
bool isNull(Key t) { return t.isNull(); };

template <>
inline bool isNull(int) { return false; };

template <>
inline bool isNull(QString string) { return string.isEmpty(); };

/**
 * Template entity which holds list of AbstractItem derived items, each corresponding to the
 * some unique key. Shares traits of associative container and list with constant time
 * random index access. Populated with keys of desired type. Items itself are created by
 * Key -> Item transformation function provided by user. Items sorting and grouping is defined
 * by ItemOrder structure. Designed to have good performance even if filled one-by-one, and
 * thousands of stored items shouldn't be a blocker for a realtime applications. However,
 * complexity of transformation function and performance of produced items is user
 * responsibility.
 * @tparam Key type, must provide operator==() and global qHash() function.
 * @note Any null non-integral keys are inacceptable.
 */
template <class Key>
class UniqueKeyListEntity: public AbstractEntity
{
    using base_type = AbstractEntity;

public:
    using KeyToItemTransform = std::function<AbstractItemPtr(const Key&)>;

    /**
     * Constructor
     * @param itemCreator Unary operation function with Key type parameter and
     *     AbstractItemPtr return type. It can't be changed during object lifetime.
     * @param itemOrder ItemOrder structure which defines strict total order. It may be
     *     changed mid-flight later which will result in items rearrange and producing necessary
     *     notifications if entity is already mapped to some model. Single item reposition caused
     *     by data change of declared role produces only move type notifications.
     */
    UniqueKeyListEntity(const KeyToItemTransform& itemCreator, const ItemOrder& itemOrder);

    /**
     * Destructor. Like any other non-composite entity, it sends remove notifications during
     * destruction if being mapped to the model. This shouldn't happen between any
     * begin_ / end_ notifications except model reset, in such case it will be just ignored.
     */
    virtual ~UniqueKeyListEntity() override;

    /**
     * Implements AbstractEntity::rowCount(). Has O(1) complexity.
     * @returns Count of elements stored within this list entity.
     */
    virtual int rowCount() const override;

    /**
     * Implements AbstractEntity::childEntity().
     * @returns nullptr.
     */
    virtual AbstractEntity* childEntity(int) const override;

    /**
     * Implements AbstractEntity::childEntityRow().
     * @returns -1.
     */
    virtual int childEntityRow(const AbstractEntity*) const override;

    /**
     * Implements AbstractEntity()::data(). Has O(1) complexity.
     * @param row Valid row index.
     * @param role Data role.
     * @returns Data for given role provided by item at given row.
     */
    virtual QVariant data(int row, int role = Qt::DisplayRole) const override;

    /**
     * Implements AbstractEntity()::flags(). Has O(1) complexity.
     * @param row Valid row index.
     * @returns Flags provided by item at row, plus Qt::ItemNeverHasChildren flag, this is
     *     list entity.
     */
    virtual Qt::ItemFlags flags(int row) const override;

    /**
     * Implements AbstractEntity()::isPlanar().
     * @returns True.
     */
    virtual bool isPlanar() const override;

    /**
     * Sets items which corresponds to result of key to item transformation done by provided
     * function. Only unique subset of keys will be used, only non-null items will be added,
     * previous contents of the list will be erased.
     * @param keys List of keys.
     */
    void setItems(const QVector<Key>& keys);

    /**
     * Adds single non null item transformed from the given key.
     * @param key The key.
     * @returns True if it succeeds, i.e item was successfully added.
     */
    bool addItem(const Key& key);

    /**
     * Moves item corresponding to the given key to the another list if such operation is
     * performable i.e this list contains such item while other list is not. Generates
     * 'move' notifications, persistent index for item won't be invalidated.
     * @note This operation is safe if performed synchronously on 'dataChanged' notification
     *     even with 'flyweight' shared items. Remove/insert approach is suitable only for truly
     *     unique items in that case.
     * @note Key -> Item transformation of other list will be bypassed. Consistency of the
     *     resulting contents is the responsibility of the caller.
     * @param key The key.
     * @param [in] otherList Same key type list, but not this.
     * @returns True if operation succeeds, i.e item was successfully moved.
     */
    bool moveItem(const Key& key, UniqueKeyListEntity<Key>* otherList);

    /**
     * Removes the item described by key if such exists.
     * @param key The key.
     * @returns True if it succeeds, i.e such item was found and removed.
     */
    bool removeItem(const Key& key);

    /**
     * Query if list contains item described by the given key. Has O(1) complexity.
     * @param key The key.
     * @returns True if such element found, false otherwise.
     */
    bool hasItem(const Key& key) const;

    /**
     * @returns True if list have zero stored items.
     */
    bool isEmpty() const;

    /**
     * Row index of the the item described by the given key.
     * @param key The key.
     * @returns Row index of item if such exists, -1 otherwise.
     */
    int itemIndex(const Key& key) const;

    /**
     * Installs the item source described by source
     * @param  source Source for the.
     */
    void installItemSource(const std::shared_ptr<UniqueKeySource<Key>>& keySource);

    /**
     * Sets item order defining structure. This lead to immediate item reordering and
     * generates layoutAboutToBeChanged() / layoutChanged() model notifications. Stays quiet
     * if actual order hasn't changed as result of operation.
     * @param itemOrder The item order defining structure.
     */
    void setItemOrder(const ItemOrder& itemOrder);

protected:

    /**
     * Sets up the item notifications
     */
    void setupItemNotifications(AbstractItem* item);

    /**
     * Local check, assures that item at the given index is positioned correctly relative to
     * direct neighbor items. Function doesn't check whole container sorting.
     * @param idx Valid item index in the sequence container.
     * @returns Non-zero value means that item is needed to be repositioned to bring sequence
     *     container back to the to valid state.
     *
     *     - -1 if item is underpositioned, larger index value expected.
     *     -  0 if item is on a right place relative to the neighbors.
     *     -  1 if item is overpositioned, less index value expected.
     */
    int sequenceItemPositionHint(int idx) const;

    /**
     * Recover item sequence order
     */
    void recoverItemSequenceOrder(int index);

protected:
    struct KeyHasher { std::size_t operator()(const Key& key) const { return ::qHash(key); } };

    std::shared_ptr<UniqueKeySource<Key>> m_keySource;
    const KeyToItemTransform m_itemCreator;
    ItemOrder m_itemOrder;

    std::unordered_map<Key, AbstractItemPtr, KeyHasher> m_keyMapping;
    std::vector<AbstractItem*> m_itemSequence;
};

/**
 * Convenient non-member factory function.
 */
template <class Key>
std::unique_ptr<UniqueKeyListEntity<Key>> makeKeyList(
    const typename UniqueKeyListEntity<Key>::KeyToItemTransform& itemCreator,
    const ItemOrder& itemOrder)
{
    return std::make_unique<UniqueKeyListEntity<Key>>(itemCreator, itemOrder);
}

} // namespace entity_item_model
} // namespace nx::vms::client::desktop
