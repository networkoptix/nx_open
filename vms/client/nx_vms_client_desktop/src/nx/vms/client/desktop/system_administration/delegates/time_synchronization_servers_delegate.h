#pragma once

#include <QtCore/QObject>

#include <QtWidgets/QStyledItemDelegate>

#include <ui/common/text_pixmap_cache.h>

namespace nx::vms::client::desktop {

class TimeSynchronizationServersDelegate: public QStyledItemDelegate
{
    using base_type = QStyledItemDelegate;

public:
    explicit TimeSynchronizationServersDelegate(QObject* parent = nullptr);

    virtual void paint(
        QPainter* painter,
        const QStyleOptionViewItem& styleOption,
        const QModelIndex& index) const override;

    virtual QSize sizeHint(
        const QStyleOptionViewItem& option,
        const QModelIndex& index) const override;

    void setBaseRow(int row);

private:
    void paintName(
        QPainter* painter,
        const QStyleOptionViewItem& styleOption,
        const QModelIndex& index) const;

    void paintTime(
        QPainter* painter,
        const QStyleOptionViewItem& styleOption,
        const QModelIndex& index) const;

private:
    mutable QnTextPixmapCache m_textPixmapCache;
    int m_baseRow = -1;
};

} // namespace nx::vms::client::desktop
