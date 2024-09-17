// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <client/client_globals.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/base_notification_observer.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/entity_common.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/item_order/item_order.h>

namespace nx::vms::client::desktop {
namespace entity_item_model {

//-------------------------------------------------------------------------------------------------
// GroupingRule  declaration.
//-------------------------------------------------------------------------------------------------

template <class GroupKey, class Key>
struct GroupingRule
{
    using KeyToGroupKeyTransform = std::function<GroupKey(const Key&, int order)>;
    using GroupKeyToItemTransform = std::function<AbstractItemPtr(const GroupKey&)>;

    KeyToGroupKeyTransform itemClassifier;
    GroupKeyToItemTransform groupKeyToItemTransform;
    int groupKeyRole;
    int dimension = 1;
    ItemOrder groupItemOrder;
};

//-------------------------------------------------------------------------------------------------
// GroupDefiningDataChangeObserver declaration.
//-------------------------------------------------------------------------------------------------

/**
 * Instances of this class are set as notification observers to the list entities which hold
 * leaf child items of some group chain. When item data provided by any of group defining roles
 * is changed, item is moved to the similar list for another group chain if needed.
 */
template <class Key>
class GroupDefiningDataChangeObserver: public BaseNotificatonObserver {
public:
    using GroupDefiningDataChangeCallback = std::function<void(const Key&)>;

    GroupDefiningDataChangeObserver(
        AbstractEntity* observedEntity,
        int keyRole,
        const QVector<int>& observedGroupDefiningRoles,
        const GroupDefiningDataChangeCallback& groupDefiningDataChangeCallback);

    virtual void dataChanged(int first, int last, const QVector<int>& roles) override;

private:
    const AbstractEntity* m_observedEntity;
    const int m_keyRole;
    const QVector<int> m_observedGroupDefiningRoles;
    GroupDefiningDataChangeCallback m_groupDefiningDataChangeCallback;
};

} // namespace entity_item_model {
} // namespace nx::vms::client::desktop
