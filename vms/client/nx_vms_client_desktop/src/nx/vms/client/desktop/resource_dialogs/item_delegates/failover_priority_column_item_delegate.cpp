// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "failover_priority_column_item_delegate.h"

#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/resource_dialogs/resource_dialogs_constants.h>

namespace {

static constexpr auto kFailoverPriorityFontWeight = QFont::DemiBold;

} // namespace

namespace nx::vms::client::desktop {

using namespace nx::vms::api;

FailoverPriorityColumnItemDelegate::FailoverPriorityColumnItemDelegate(QObject* parent):
    base_type(parent)
{
}

void FailoverPriorityColumnItemDelegate::initStyleOption(
    QStyleOptionViewItem* option,
    const QModelIndex& index) const
{
    base_type::initStyleOption(option, index);

    const auto failoverPriorityData = index.data(ResourceDialogItemRole::FailoverPriorityRole);
    if (failoverPriorityData.isNull())
        return;

    const auto failoverPriority = failoverPriorityData.value<FailoverPriority>();

    option->text = failoverPriorityToString(failoverPriority);
    option->features.setFlag(QStyleOptionViewItem::HasDisplay);
    option->displayAlignment = Qt::AlignRight | Qt::AlignVCenter;

    option->font.setWeight(kFailoverPriorityFontWeight);
    option->fontMetrics = QFontMetrics(option->font);

    const auto failoverPriorityColor = failoverPriorityToColor(failoverPriority);
    option->palette.setColor(QPalette::Text, failoverPriorityColor);
    option->palette.setColor(QPalette::HighlightedText, failoverPriorityColor);
}

QString FailoverPriorityColumnItemDelegate::failoverPriorityToString(
    FailoverPriority failoverPriority) const
{
    switch (failoverPriority)
    {
        case FailoverPriority::never:
            return tr("No Failover");
        case FailoverPriority::low:
            return tr("Low");
        case FailoverPriority::medium:
            return tr("Medium");
        case FailoverPriority::high:
            return tr("High");

        default:
            NX_ASSERT(false);
            break;
    }
    return {};
}

QColor FailoverPriorityColumnItemDelegate::failoverPriorityToColor(
    FailoverPriority failoverPriority) const
{
    static const QColor kNeverColor = core::colorTheme()->color("failoverPriority.never");
    static const QColor kLowColor = core::colorTheme()->color("failoverPriority.low");
    static const QColor kMediumColor = core::colorTheme()->color("failoverPriority.medium");
    static const QColor kHighColor = core::colorTheme()->color("failoverPriority.high");

    switch (failoverPriority)
    {
        case FailoverPriority::never:
            return kNeverColor;
        case FailoverPriority::low:
            return kLowColor;
        case FailoverPriority::medium:
            return kMediumColor;
        case FailoverPriority::high:
            return kHighColor;

        default:
            NX_ASSERT(false);
            break;
    }

    return {};
}

} // namespace nx::vms::client::desktop
