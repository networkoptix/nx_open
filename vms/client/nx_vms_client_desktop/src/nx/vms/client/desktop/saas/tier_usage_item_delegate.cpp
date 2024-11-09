// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "tier_usage_item_delegate.h"

#include <nx/vms/client/desktop/saas/tier_usage_common.h>
#include <nx/vms/client/desktop/saas/tier_usage_model.h>
#include <nx/vms/client/desktop/style/custom_style.h>

namespace {

static constexpr int kTierUsageTableMinimumColumnWidth = 100;

} // namespace

namespace nx::vms::client::desktop::saas {

TierUsageItemDelegate::TierUsageItemDelegate(QObject* parent):
    base_type(parent)
{
}

QSize TierUsageItemDelegate::sizeHint(
    const QStyleOptionViewItem& option,
    const QModelIndex& index) const
{
    auto result = base_type::sizeHint(option, index);
    result.rwidth() = std::max(result.width(), kTierUsageTableMinimumColumnWidth);
    return result;
}

void TierUsageItemDelegate::initStyleOption(
    QStyleOptionViewItem* option,
    const QModelIndex& index) const
{
    base_type::initStyleOption(option, index);

    // Keyboard focus decoration is never displayed across client item views.
    option->state.setFlag(QStyle::State_HasFocus, false);

    if (index.column() == TierUsageModel::LimitTypeColumn)
    {
        const auto tierLimitTypeData = index.data(TierUsageModel::LimitTypeRole);
        if (!tierLimitTypeData.isNull())
        {
            const auto tierLimitType =
                static_cast<nx::vms::api::SaasTierLimitName>(tierLimitTypeData.toInt());

            if (isBooleanTierLimitType(tierLimitType))
                setWarningStyle(&option->palette);
        }
    }

    if (index.column() == TierUsageModel::UsedLimitColumn)
        setWarningStyle(&option->palette);
}

} // namespace nx::vms::client::desktop::saas
