#include "update_status_item_delegate.h"

#include <QtWidgets/QStyleOptionProgressBar>
#include <QtWidgets/QApplication>

#include <ui/models/server_updates_model.h>
#include <utils/media_server_update_tool.h>

QnUpdateStatusItemDelegate::QnUpdateStatusItemDelegate(QWidget *parent) :
    QStyledItemDelegate(parent)
{
}

QnUpdateStatusItemDelegate::~QnUpdateStatusItemDelegate() {}

void QnUpdateStatusItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    int state = index.data(QnServerUpdatesModel::StateRole).toInt();
    if (state == QnMediaServerUpdateTool::PeerUpdateInformation::UpdateDownloading ||
        state == QnMediaServerUpdateTool::PeerUpdateInformation::UpdateUploading)
    {
        QStyleOptionProgressBar progressBarStyleOption;
        progressBarStyleOption.rect = option.rect.adjusted(4, 4, -4, -4);

        const int progress = index.data(QnServerUpdatesModel::ProgressRole).toInt();

        progressBarStyleOption.minimum = 0;
        progressBarStyleOption.maximum = 100;
        progressBarStyleOption.textAlignment = Qt::AlignCenter;
        progressBarStyleOption.progress = progress ;
        progressBarStyleOption.text = QString(lit("%1%")).arg(progress);
        progressBarStyleOption.textVisible = true;

        QApplication::style()->drawControl(QStyle::CE_ProgressBar, &progressBarStyleOption, painter, option.widget);
    } else {
        base_type::paint(painter, option, index);
    }
}

void QnUpdateStatusItemDelegate::initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const {
    base_type::initStyleOption(option, index);
}
