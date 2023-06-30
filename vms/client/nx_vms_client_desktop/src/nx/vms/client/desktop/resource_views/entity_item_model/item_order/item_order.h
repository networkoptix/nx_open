// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <QtCore/QVector>

namespace nx::vms::client::desktop {
namespace entity_item_model {

class AbstractItem;
class GroupEntity;

struct NX_VMS_CLIENT_DESKTOP_API ItemOrder
{
    /**
     * Function wrapper which defines strict total order: in current implementation lhs and
     * rhs pointer values are guaranteed to be different (i.e all items in the list are
     * unique). This is necessary requirement for any other sorting comparator
     * implementation, thus, in case of equality fallback to pointers comparison is
     * mandatory. This trait makes sorting inherently stable as well.
     */
    using Comp = std::function<bool(const AbstractItem* lhs, const AbstractItem* rhs)>;
    Comp comp;

    /**
     * Data roles relevant to sorting. Default implementation doesn't depend on any stored
     * data. For any implementation which uses data acquired by certain role, that role
     * should be provided to trigger item order restore on data change notification. Please
     * keep in mind, that providing core::ResourceRole isn't choice if you going compare
     * resource properties, not pointers itself. Mutable sorting keys are quick way to end
     * with item lookup failure, and, as result, corrupted state of the storage.
     */
    using Roles = QVector<int>;
    Roles roles;
};

/**
 * Provides most achievable item list performance. Item order is arbitrary and undefined,
 * based on sorted item pointer sequence. Supposed to use with an sorting proxy model. Any
 * other order implementation may fallback to noOrder() since it always guarantees strict
 * total order.
 */
NX_VMS_CLIENT_DESKTOP_API ItemOrder noOrder();

/**
 * Provides alphanumeric order based on displayed text.
 * @param ascending (Optional) True for ascending order, false for descending.
 * @param caseSensitivity Case sensitivity.
 */
NX_VMS_CLIENT_DESKTOP_API ItemOrder numericOrder(
    Qt::SortOrder sortOrder = Qt::AscendingOrder,
    Qt::CaseSensitivity caseSensitivity = Qt::CaseInsensitive);

} // namespace entity_item_model
} // namespace nx::vms::client::desktop
