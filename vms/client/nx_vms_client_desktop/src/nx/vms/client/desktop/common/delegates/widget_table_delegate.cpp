#include "widget_table_delegate.h"

#include <QtWidgets/QLabel>

#include <ui/common/palette.h>
#include <ui/style/helper.h>

#include <nx/vms/client/desktop/common/widgets/private/widget_table_p.h>

namespace nx::vms::client::desktop {

WidgetTableDelegate::WidgetTableDelegate(QObject* parent): base_type(parent)
{
}

QWidget* WidgetTableDelegate::createWidget(
    QAbstractItemModel* /*model*/, const QModelIndex& /*index*/, QWidget* parent) const
{
    auto label = new QLabel(parent);
    label->setIndent(style::Metrics::kStandardPadding);
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    label->setWordWrap(false);
    return label;
}

bool WidgetTableDelegate::updateWidget(QWidget* widget, const QModelIndex& index) const
{
    if (!widget)
        return false;

    widget->setToolTip(index.data(Qt::ToolTipRole).toString());
    widget->setStatusTip(index.data(Qt::StatusTipRole).toString());
    widget->setWhatsThis(index.data(Qt::WhatsThisRole).toString());

    const auto foreground = index.data(Qt::ForegroundRole);
    if (foreground.canConvert<QBrush>())
        setPaletteBrush(widget, widget->foregroundRole(), foreground.value<QBrush>());

    const auto background = index.data(Qt::BackgroundRole);
    if (background.canConvert<QBrush>())
        setPaletteBrush(widget, widget->backgroundRole(), background.value<QBrush>());

    const auto label = qobject_cast<QLabel*>(widget);
    if (!label)
        return false;

    label->setText(index.data(Qt::DisplayRole).toString());
    return true;
}

QnIndents WidgetTableDelegate::itemIndents(QWidget* /*widget*/, const QModelIndex& /*index*/) const
{
    return QnIndents(0, style::Metrics::kStandardPadding);
}

QSize WidgetTableDelegate::sizeHint(QWidget* widget, const QModelIndex& /*index*/) const
{
    return (widget ? widget->sizeHint() : QSize());
}

QModelIndex WidgetTableDelegate::indexForWidget(QWidget* widget)
{
    return WidgetTable::indexForWidget(widget);
}

} // namespace nx::vms::client::desktop
