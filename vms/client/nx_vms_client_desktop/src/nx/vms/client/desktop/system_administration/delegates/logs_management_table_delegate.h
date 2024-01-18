// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtWidgets/QStyledItemDelegate>

#include <ui/common/text_pixmap_cache.h>

namespace nx::vms::client::desktop {

class LogsManagementTableDelegate: public QStyledItemDelegate
{
    using base_type = QStyledItemDelegate;

public:
    explicit LogsManagementTableDelegate(QObject* parent = nullptr);

    virtual void paint(
        QPainter* painter,
        const QStyleOptionViewItem& styleOption,
        const QModelIndex& index) const override;

private:
    void paintNameColumn(
        QPainter* painter,
        const QStyleOptionViewItem& styleOption,
        const QModelIndex& index) const;

    void paintCheckBoxColumn(
        QPainter* painter,
        const QStyleOptionViewItem& styleOption,
        const QModelIndex& index) const;

    void paintLogLevelColumn(
        QPainter* painter,
        const QStyleOptionViewItem& styleOption,
        const QModelIndex& index) const;

    void drawText(const QModelIndex& index, QRect textRect, QStyle* style, QStyleOptionViewItem option, QPainter* painter, bool extra) const;

private:
    mutable QnTextPixmapCache m_textPixmapCache;
};

} // namespace nx::vms::client::desktop
