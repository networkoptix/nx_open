#include "update_status_item_delegate.h"

#include <QtWidgets/QStyleOptionProgressBar>
#include <QtWidgets/QApplication>

#include <ui/models/server_updates_model.h>

namespace {

    const int kDefaultWidth = 200;

    const int kProgressMargin = 4;

} // namespace

QnUpdateStatusItemDelegate::QnUpdateStatusItemDelegate(QWidget* parent) :
    QStyledItemDelegate(parent)
{
}

QnUpdateStatusItemDelegate::~QnUpdateStatusItemDelegate()
{
}

void QnUpdateStatusItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QnPeerUpdateStage state = (QnPeerUpdateStage)index.data(QnServerUpdatesModel::StageRole).toInt();
    switch (state)
    {
        case QnPeerUpdateStage::Download:
        case QnPeerUpdateStage::Push:
        case QnPeerUpdateStage::Install:
        {
            paintProgressBar(painter, option, index);
            break;
        }

        default:
        {
            base_type::paint(painter, option, index);
            break;
        }
    }
}

void QnUpdateStatusItemDelegate::paintProgressBar(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionProgressBar progressBarStyleOption;
    progressBarStyleOption.rect = option.rect.adjusted(
        kProgressMargin, kProgressMargin, -kProgressMargin, -kProgressMargin);

    const int progress = index.data(QnServerUpdatesModel::ProgressRole).toInt();

    progressBarStyleOption.minimum = 0;
    progressBarStyleOption.maximum = 100;
    progressBarStyleOption.progress = progress;
    progressBarStyleOption.textVisible = false;

    auto style = option.widget ? option.widget->style() : qApp->style();
    style->drawControl(QStyle::CE_ProgressBar, &progressBarStyleOption, painter, option.widget);
}

QSize QnUpdateStatusItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QSize result = base_type::sizeHint(option, index);
    result.setWidth(qMax(result.width(), kDefaultWidth));
    return result;
}


