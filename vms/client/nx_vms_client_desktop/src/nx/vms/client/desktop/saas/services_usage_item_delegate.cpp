// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "services_usage_item_delegate.h"

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/saas/services_usage_model.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/helper.h>

namespace {

static constexpr int kServiceUsageTableMinimumColumnWidth = 100;

} // namespace

namespace nx::vms::client::desktop::saas {

ServicesUsageItemDelegate::ServicesUsageItemDelegate(QObject* parent):
    base_type(parent)
{
}

QSize ServicesUsageItemDelegate::sizeHint(
    const QStyleOptionViewItem& option,
    const QModelIndex& index) const
{
    auto result = base_type::sizeHint(option, index);

    if (index.column() == ServicesUsageModel::ServiceOveruseWarningIconColumn)
    {
        result.rwidth() = !index.data(Qt::DecorationRole).isNull()
            ? nx::style::Metrics::kDefaultIconSize + nx::style::Metrics::kStandardPadding
            : 0;
    }
    else
    {
        result.rwidth() = std::max(result.width(), kServiceUsageTableMinimumColumnWidth);
    }

    return result;
}

void ServicesUsageItemDelegate::initStyleOption(
    QStyleOptionViewItem* option,
    const QModelIndex& index) const
{
    base_type::initStyleOption(option, index);

    switch (index.column())
    {
        case ServicesUsageModel::ServiceOveruseWarningIconColumn:
            option->displayAlignment = Qt::AlignRight | Qt::AlignVCenter;
            break;

        case ServicesUsageModel::TotalQantityColumn:
        case ServicesUsageModel::UsedQantityColumn:
            if (index.data(ServicesUsageModel::ServiceOverusedRole).toBool())
                setWarningStyle(&option->palette);
            break;

        default:
            break;
    }
}

} // namespace nx::vms::client::desktop::saas
