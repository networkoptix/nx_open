// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "item_order.h"

#include <QtCore/QCollator>
#include <nx/vms/client/desktop/resource_views/entity_item_model/item/abstract_item.h>

namespace nx::vms::client::desktop {
namespace entity_item_model {

ItemOrder noOrder()
{
    return {std::less<>{}, {}};
}

ItemOrder numericOrder(
    Qt::SortOrder sortOrder /*= Qt::AscendingOrder*/,
    Qt::CaseSensitivity caseSensitivity /*= Qt::CaseInsensitive*/)
{
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(caseSensitivity);

    return {
        [collator, sortOrder](const AbstractItem* lhs, const AbstractItem* rhs)
        {
            const auto collatorCompareResult = collator.compare(
                lhs->data(Qt::DisplayRole).toString(), rhs->data(Qt::DisplayRole).toString());

            if (collatorCompareResult == 0)
                return sortOrder == Qt::AscendingOrder ? (lhs < rhs) : (lhs > rhs);

            return sortOrder == Qt::AscendingOrder
                ? (collatorCompareResult < 0)
                : (collatorCompareResult > 0);
        },
        {Qt::DisplayRole}
    };
}

} // namespace entity_item_model
} // namespace nx::vms::client::desktop
