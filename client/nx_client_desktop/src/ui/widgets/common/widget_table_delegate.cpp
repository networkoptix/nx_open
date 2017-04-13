#include "widget_table_delegate.h"
#include "widget_table_p.h"

#include <QtWidgets/QLabel>

#include <ui/common/palette.h>
#include <ui/style/helper.h>


QnWidgetTableDelegate::QnWidgetTableDelegate(QObject* parent):
    base_type(parent)
{
}

QWidget* QnWidgetTableDelegate::createWidget(
    QAbstractItemModel* model, const QModelIndex& index, QWidget* parent) const
{
    Q_UNUSED(index);
    Q_UNUSED(model);

    auto label = new QLabel(parent);
    label->setIndent(style::Metrics::kStandardPadding);
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    label->setWordWrap(false);
    return label;
}

bool QnWidgetTableDelegate::updateWidget(QWidget* widget, const QModelIndex& index) const
{
    if (!widget)
        return false;

    widget->setToolTip(index.data(Qt::ToolTipRole).toString());
    widget->setStatusTip(index.data(Qt::StatusTipRole).toString());
    widget->setWhatsThis(index.data(Qt::WhatsThisRole).toString());

    auto foreground = index.data(Qt::ForegroundRole);
    if (foreground.canConvert<QBrush>())
        setPaletteBrush(widget, widget->foregroundRole(), foreground.value<QBrush>());

    auto background = index.data(Qt::BackgroundRole);
    if (background.canConvert<QBrush>())
        setPaletteBrush(widget, widget->backgroundRole(), background.value<QBrush>());

    auto label = qobject_cast<QLabel*>(widget);
    if (!label)
        return false;

    label->setText(index.data(Qt::DisplayRole).toString());
    return true;
}

QnIndents QnWidgetTableDelegate::itemIndents(QWidget* widget, const QModelIndex& index) const
{
    Q_UNUSED(widget);
    Q_UNUSED(index);
    return QnIndents(0, style::Metrics::kStandardPadding);
}

QSize QnWidgetTableDelegate::sizeHint(QWidget* widget, const QModelIndex& index) const
{
    Q_UNUSED(index);
    return (widget ? widget->sizeHint() : QSize());
}

QModelIndex QnWidgetTableDelegate::indexForWidget(QWidget* widget)
{
    return QnWidgetTablePrivate::indexForWidget(widget);
}
