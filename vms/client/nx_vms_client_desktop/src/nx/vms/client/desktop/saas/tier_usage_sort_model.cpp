// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "tier_usage_sort_model.h"

#include <QtCore/QCollator>

#include <nx/vms/client/desktop/saas/tier_usage_model.h>

namespace nx::vms::client::desktop::saas {

TierUsageSortModel::TierUsageSortModel(QObject* parent):
    base_type(parent)
{
}

bool TierUsageSortModel::lessThan(const QModelIndex& lhs, const QModelIndex& rhs) const
{
    const auto lhsTierLimitType = lhs.data(TierUsageModel::LimitTypeRole);
    const auto rhsTierLimitType = rhs.data(TierUsageModel::LimitTypeRole);

    if (!lhsTierLimitType.isNull() && !rhsTierLimitType.isNull())
        return lhsTierLimitType.toInt() < rhsTierLimitType.toInt();

    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    collator.setNumericMode(true);
    return collator.compare(
        lhs.data(Qt::DisplayRole).toString(), rhs.data(Qt::DisplayRole).toString()) < 0;
}

} // namespace nx::vms::client::desktop::saas
