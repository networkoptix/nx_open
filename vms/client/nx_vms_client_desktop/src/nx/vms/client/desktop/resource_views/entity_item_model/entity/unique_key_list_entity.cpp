// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "unique_key_list_entity.h"

#include <nx/vms/client/desktop/resource_views/entity_item_model/entity_model_mapping.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/entity_notification_guard.h>
#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {
namespace entity_item_model {

template <class Key>
UniqueKeyListEntity<Key>::UniqueKeyListEntity(
    const KeyToItemTransform& itemCreator,
    const ItemOrder& itemOrder)
    :
    base_type(),
    m_itemCreator(itemCreator),
    m_itemOrder(itemOrder)
{
}

template <class Key>
UniqueKeyListEntity<Key>::~UniqueKeyListEntity()
{
    auto guard = removeRowsGuard(modelMapping(), 0, rowCount());
    m_itemSequence.clear();
    m_keyMapping.clear();
}

template <class Key>
int UniqueKeyListEntity<Key>::rowCount() const
{
    return static_cast<int>(m_itemSequence.size());
}

template <class Key>
AbstractEntity* UniqueKeyListEntity<Key>::childEntity(int) const
{
    return nullptr;
}

template <class Key>
int UniqueKeyListEntity<Key>::childEntityRow(const AbstractEntity*) const
{
    return -1;
}

template <class Key>
QVariant UniqueKeyListEntity<Key>::data(int row, int role /*= Qt::DisplayRole*/) const
{
    if (row < 0 || row >= rowCount())
    {
        NX_ASSERT(false, "Invalid row index");
        return QVariant();
    }
    return m_itemSequence[row]->data(role);
}

template <class Key>
Qt::ItemFlags UniqueKeyListEntity<Key>::flags(int row) const
{
    if (row < 0 || row >= rowCount())
    {
        NX_ASSERT(false, "Invalid row index");
        return Qt::ItemFlags();
    }
    return m_itemSequence[row]->flags() | Qt::ItemNeverHasChildren;
}

template <class Key>
bool UniqueKeyListEntity<Key>::isPlanar() const
{
    return true;
}

template <class Key>
void UniqueKeyListEntity<Key>::setItems(const QVector<Key>& keys)
{
    if (!isEmpty())
    {
        auto guard = removeRowsGuard(modelMapping(), 0, rowCount());
        m_keyMapping.clear();
        m_itemSequence.clear();
    }

    if (keys.isEmpty())
        return;

    decltype(m_itemSequence) newItemSequence;
    newItemSequence.reserve(keys.size());

    for (const auto& key: keys)
    {
        if (hasItem(key) || isNull(key))
            continue;

        auto createdItem = m_itemCreator(key);
        if (!createdItem)
        {
            NX_ASSERT("Invalid Key or Key->Item transformation provided, null item created.");
            continue;
        }
        auto keyItr = m_keyMapping.insert(std::make_pair(key, std::move(createdItem)));
        newItemSequence.push_back(keyItr.first->second.get());
    }
    if (newItemSequence.empty())
        return;

    std::sort(std::begin(newItemSequence), std::end(newItemSequence), m_itemOrder.comp);

    auto guard = insertRowsGuard(modelMapping(), 0, newItemSequence.size());

    m_itemSequence = std::move(newItemSequence);
    for (auto item: m_itemSequence)
        setupItemNotifications(item);
}

template <class Key>
bool UniqueKeyListEntity<Key>::addItem(const Key& key)
{
    if (isNull(key))
        return false;

    if (hasItem(key))
        return false;

    auto createdItem = m_itemCreator(key);
    if (!createdItem)
    {
        NX_ASSERT("Invalid Key or Key->Item transformation provided, null item created.");
        return false;
    }

    auto keyItr = m_keyMapping.insert(std::make_pair(key, std::move(createdItem)));
    AbstractItem* item = keyItr.first->second.get();

    const auto itemSequenceItr = std::lower_bound(
        std::cbegin(m_itemSequence), std::cend(m_itemSequence), item, m_itemOrder.comp);
    const int itemIndex = std::distance(std::cbegin(m_itemSequence), itemSequenceItr);

    auto guard = insertRowsGuard(modelMapping(), itemIndex);
    m_itemSequence.insert(itemSequenceItr, item);
    setupItemNotifications(item);

    return true;
}

template <class Key>
bool UniqueKeyListEntity<Key>::moveItem(const Key& key, UniqueKeyListEntity<Key>* otherList)
{
    if (isNull(key) || !otherList)
        return false;

    // One of two following conditions will met on attempt to move item to the same list.
    if (otherList->hasItem(key))
        return false;

    auto keyItr = m_keyMapping.find(key);
    if (keyItr == std::cend(m_keyMapping))
        return false;

    // Get index of item within this list.
    // TODO: #vbreus define order of notifications.
    AbstractItem* item = keyItr->second.get();
    auto itemSequenceItr =
        std::find(std::cbegin(m_itemSequence), std::cend(m_itemSequence), item);
    const int itemIndex = std::distance(std::cbegin(m_itemSequence), itemSequenceItr);

    // Get potential position of item within other list.
    auto otherSequenceItr = std::lower_bound(std::cbegin(otherList->m_itemSequence),
        std::cend(otherList->m_itemSequence), item, otherList->m_itemOrder.comp);
    const int otherItemIndex =
        std::distance(std::cbegin(otherList->m_itemSequence), otherSequenceItr);

    // TODO: #vbreus Notification observers of other list won't get move notification.
    // It's first world problem now, but should be fixed to bring expected behavior.
    auto guard = moveRowsGuard(modelMapping(), this, itemIndex, 1, otherList, otherItemIndex);

    otherList->m_keyMapping.emplace(key, std::move(keyItr->second));
    otherList->m_itemSequence.insert(otherSequenceItr, item);
    otherList->setupItemNotifications(item);

    m_keyMapping.erase(keyItr);
    m_itemSequence.erase(itemSequenceItr);

    return true;
}

template <class Key>
bool UniqueKeyListEntity<Key>::removeItem(const Key& key)
{
    auto keyItr = m_keyMapping.find(key);
    if (keyItr == std::cend(m_keyMapping))
        return false;

    AbstractItem* item = keyItr->second.get();
    auto itemSequenceItr = std::find(std::cbegin(m_itemSequence), std::cend(m_itemSequence), item);
    const int index = std::distance(std::cbegin(m_itemSequence), itemSequenceItr);

    auto guard = removeRowsGuard(modelMapping(), index);
    m_keyMapping.erase(keyItr);
    m_itemSequence.erase(itemSequenceItr);

    return true;
}

template <class Key>
bool UniqueKeyListEntity<Key>::hasItem(const Key& key) const
{
    return m_keyMapping.find(key) != std::cend(m_keyMapping);
}

template <class Key>
bool UniqueKeyListEntity<Key>::isEmpty() const
{
    return rowCount() == 0;
}

template <class Key>
int UniqueKeyListEntity<Key>::itemIndex(const Key& key) const
{
    auto keyItr = m_keyMapping.find(key);
    if (keyItr == std::cend(m_keyMapping))
        return -1;

    AbstractItem* item = keyItr->second.get();
    const auto itemSequenceItr = std::lower_bound(
        std::cbegin(m_itemSequence), std::cend(m_itemSequence), item, m_itemOrder.comp);
    return std::distance(std::cbegin(m_itemSequence), itemSequenceItr);
}

template <class Key>
void UniqueKeyListEntity<Key>::installItemSource(
    const std::shared_ptr<UniqueKeySource<Key>>& keySource)
{
    m_keySource = keySource;
    m_keySource->setKeysHandler =
        [this](const QVector<Key>& keys) { setItems(keys); };

    using KeyNotifyHandler = typename UniqueKeySource<Key>::KeyNotifyHandler;

    m_keySource->addKeyHandler = std::make_shared<KeyNotifyHandler>(
        [this](const Key& key) { addItem(key); });

    m_keySource->removeKeyHandler = std::make_shared<KeyNotifyHandler>(
        [this](const Key& key) { removeItem(key); });

    m_keySource->initializeRequest();
}

template <class Key>
void UniqueKeyListEntity<Key>::setItemOrder(const ItemOrder& itemOrder)
{
    auto sequentalGenerator = [n = 0]() mutable { return n++; };
    std::vector<std::pair<AbstractItem*, int>> permutationSolver;
    permutationSolver.reserve(m_itemSequence.size());
    for (const auto item: m_itemSequence)
        permutationSolver.emplace_back(item, sequentalGenerator());

    std::sort(std::begin(permutationSolver), std::end(permutationSolver),
        [&itemOrder](const auto& lhs, const auto& rhs)
        {
            return itemOrder.comp(lhs.first, rhs.first);
        });

    // If resulting permutation is sorted in ascending order, that means that order hasn't
    // changed and no notifications needed.
    if (std::is_sorted(std::cbegin(permutationSolver), std::cend(permutationSolver),
        [](const auto& lhs, const auto& rhs) { return lhs.second < rhs.second; }))
        {
            m_itemOrder = itemOrder;
            return;
        }

    std::vector<int> permutation(permutationSolver.size(), 0);
    std::transform(std::cbegin(permutationSolver), std::cend(permutationSolver),
        std::begin(permutation), [](const auto& itemAndIndex)
        {
            return itemAndIndex.second;
        });

    auto guard = layoutChangeGuard(modelMapping(), permutation);
    m_itemOrder = itemOrder;
    std::transform(std::cbegin(permutationSolver), std::cend(permutationSolver),
        std::begin(m_itemSequence), [](const auto& itemAndIndex)
        {
            return itemAndIndex.first;
        });
}

template <class Key>
void UniqueKeyListEntity<Key>::setupItemNotifications(AbstractItem* item)
{
    item->setDataChangedCallback(
        [this, item](const QVector<int>& roles)
        {
            const auto matchingRoleItr = std::find_first_of(
                std::cbegin(roles), std::cend(roles),
                std::cbegin(m_itemOrder.roles), std::cend(m_itemOrder.roles));

            const bool reorderExpected = matchingRoleItr != std::cend(roles);

            if (!reorderExpected)
            {
                auto itemItr = std::lower_bound(std::cbegin(m_itemSequence),
                    std::cend(m_itemSequence), item, m_itemOrder.comp);

                const int itemIndex = std::distance(std::cbegin(m_itemSequence), itemItr);
                dataChanged(modelMapping(), roles, itemIndex);
            }
            else
            {
                auto itemItr =
                    std::find(std::begin(m_itemSequence), std::end(m_itemSequence), item);
                int itemIndex = std::distance(std::begin(m_itemSequence), itemItr);
                dataChanged(modelMapping(), roles, itemIndex);

                // It's possible if was moved or removed on 'data changed' notification.
                // TODO: #vbreus An attempt to add item should fail if done within
                // this callback scope.
                if (itemIndex >= m_itemSequence.size() || m_itemSequence[itemIndex] != item)
                    return;

                recoverItemSequenceOrder(itemIndex);
            }
        });
}

template <class Key>
int UniqueKeyListEntity<Key>::sequenceItemPositionHint(int idx) const
{
    const auto lastIndex = static_cast<int>(m_itemSequence.size()) - 1;

    if (lastIndex < 1)
        return 0;

    if (idx == 0)
        return m_itemOrder.comp(m_itemSequence[idx], m_itemSequence[idx + 1]) ? 0 : -1;

    if (idx == lastIndex)
        return m_itemOrder.comp(m_itemSequence[idx - 1], m_itemSequence[idx]) ? 0 : 1;

    if (!m_itemOrder.comp(m_itemSequence[idx - 1], m_itemSequence[idx]))
        return 1;

    if (!m_itemOrder.comp(m_itemSequence[idx], m_itemSequence[idx + 1]))
        return -1;

    return 0;

    // Predicating: If item A isn't less than item B that should explicitly mean that item A
    // is greater than item B. Comparison transitivity is expected. It's hard to imagine that
    // A and B is not unique things. From this it follows that we call strict total order.
    // Strict weak order comparators isn't applicable here.

    // Expected that all pointers in m_itemSequenceContainer are unique and not null.
    // That's easy to maintain since items are unique and owned by entity by design. This
    // assert most probably occur if there is no fallback to pointers comparison in case of
    // equality deduced from previous comparisons in NodeOrder template parameter operator<
    // implementation. Please do it.

    NX_ASSERT(false, "Strict total order expected");

    return 0;
}

template <class Key>
void UniqueKeyListEntity<Key>::recoverItemSequenceOrder(int index)
{
    const int positionHint = sequenceItemPositionHint(index);
    if (positionHint == 0)
        return;

    auto itemItr = std::next(std::begin(m_itemSequence), index);
    auto item = m_itemSequence.at(index);

    if (m_itemSequence.size() == 2)
    {
        // Expected moveRows(0, 2) or moveRows(1, 0). Meaning is: "move item at index N to
        // new place, before index M, relating to state before operation".
        auto guard = moveRowsGuard(modelMapping(), this, index, 1, this, 1 - positionHint);

        std::iter_swap(itemItr, itemItr - positionHint);
        return;
    }

    if (positionHint < 0)
    {
        auto targetItr = std::lower_bound(
            std::next(itemItr), std::end(m_itemSequence), item, m_itemOrder.comp);
        int targetIndex = std::distance(std::begin(m_itemSequence), targetItr);

        auto guard = moveRowsGuard(modelMapping(), this, index, 1, this, targetIndex);
        std::rotate(itemItr, std::next(itemItr), targetItr);
    }
    else
    {
        auto targetItr = std::lower_bound(
            std::begin(m_itemSequence), itemItr, item, m_itemOrder.comp);
        const int targetIndex = std::distance(std::begin(m_itemSequence), targetItr);

        auto guard = moveRowsGuard(modelMapping(), this, index, 1, this, targetIndex);
        std::rotate(std::prev(std::make_reverse_iterator(itemItr)),
            std::make_reverse_iterator(itemItr), std::make_reverse_iterator(targetItr));
    }
}

//-------------------------------------------------------------------------------------------------
// UniqueKeyListEntity explicit instantiations.
//-------------------------------------------------------------------------------------------------

template class NX_VMS_CLIENT_DESKTOP_API UniqueKeyListEntity<QnResourcePtr>;
template class NX_VMS_CLIENT_DESKTOP_API UniqueKeyListEntity<nx::Uuid>;
template class NX_VMS_CLIENT_DESKTOP_API UniqueKeyListEntity<QString>;
template class NX_VMS_CLIENT_DESKTOP_API UniqueKeyListEntity<int>;

} // namespace entity_item_model
} // namespace nx::vms::client::desktop
