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
    mutable QnTextPixmapCache m_textPixmapCache;
};

} // namespace nx::vms::client::desktop
