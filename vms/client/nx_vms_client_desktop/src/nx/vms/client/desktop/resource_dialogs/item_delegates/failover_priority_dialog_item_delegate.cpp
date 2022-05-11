// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "failover_priority_dialog_item_delegate.h"

#include <QtWidgets/QApplication>

#include <client/client_globals.h>
#include <nx/vms/client/desktop/common/widgets/tree_view.h>
#include <nx/vms/client/desktop/node_view/details/node/view_node_helper.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <ui/common/indents.h>

#include <QtGui/QPainter>

namespace {

static constexpr auto kFailoverPriorityFontWeight = (QFont::DemiBold + QFont::Bold) / 2;

} // namespace

namespace nx::vms::client::desktop {

using namespace nx::vms::api;

FailoverPriorityDialogItemDelegate::FailoverPriorityDialogItemDelegate(QObject* parent):
    base_type(parent)
{
}

void FailoverPriorityDialogItemDelegate::paint(
    QPainter* painter,
    const QStyleOptionViewItem& styleOption,
    const QModelIndex& index) const
{
    base_type::paint(painter, styleOption, index);

    if (isSeparator(index) || isSpacer(index))
        return;

    paintFailoverPriorityString(painter, styleOption, index);
}

QString FailoverPriorityDialogItemDelegate::failoverPriorityToString(
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
            break;
    }
    return QString();
}

QColor FailoverPriorityDialogItemDelegate::failoverPriorityToColor(
    FailoverPriority failoverPriority) const
{
    static const QColor kNeverColor = colorTheme()->color("failoverPriority.never");
    static const QColor kLowColor = colorTheme()->color("failoverPriority.low");
    static const QColor kMediumColor = colorTheme()->color("failoverPriority.medium");
    static const QColor kHighColor = colorTheme()->color("failoverPriority.high");

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
            break;
    }
    return QColor();
}

void FailoverPriorityDialogItemDelegate::paintFailoverPriorityString(
    QPainter* painter,
    const QStyleOptionViewItem& styleOption,
    const QModelIndex& index) const
{
    const auto failoverPriorityData = index.data(Qn::FailoverPriorityRole);
    if (failoverPriorityData.isNull())
        return;

    const auto failoverPriority = failoverPriorityData.value<FailoverPriority>();
    QStyleOptionViewItem failoverOption(styleOption);

    failoverOption.state.setFlag(QStyle::State_MouseOver, false);
    failoverOption.state.setFlag(QStyle::State_Selected, false);
    failoverOption.text = failoverPriorityToString(failoverPriority);

    failoverOption.font.setWeight(kFailoverPriorityFontWeight);
    failoverOption.fontMetrics = QFontMetrics(failoverOption.font);

    failoverOption.palette.setColor(QPalette::Text, failoverPriorityToColor(failoverPriority));
    failoverOption.displayAlignment = Qt::AlignRight | Qt::AlignVCenter;

    const auto widget = failoverOption.widget;
    const auto style = widget ? widget->style() : QApplication::style();

    style->drawControl(QStyle::CE_ItemViewItem, &failoverOption, painter, widget);
}

int FailoverPriorityDialogItemDelegate::decoratorWidth(const QModelIndex& index) const
{
    const auto failoverPriorityData = index.data(Qn::FailoverPriorityRole);
    if (failoverPriorityData.isNull())
        return 0;

    const auto failoverPriority = failoverPriorityData.value<FailoverPriority>();

    QStyleOptionViewItem option;
    option.font.setWeight(kFailoverPriorityFontWeight);
    option.fontMetrics = QFontMetrics(option.font);

    return option.fontMetrics.width(failoverPriorityToString(failoverPriority));
}

} // namespace nx::vms::client::desktop
