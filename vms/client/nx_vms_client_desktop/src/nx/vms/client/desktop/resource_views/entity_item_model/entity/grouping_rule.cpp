// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "grouping_rule.h"

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

template class NX_VMS_CLIENT_DESKTOP_API GroupDefiningDataChangeObserver<QnResourcePtr>;
template class NX_VMS_CLIENT_DESKTOP_API GroupDefiningDataChangeObserver<int>;
template class NX_VMS_CLIENT_DESKTOP_API GroupDefiningDataChangeObserver<nx::Uuid>;

} // namespace entity_item_model
} // namespace nx::vms::client::desktop
