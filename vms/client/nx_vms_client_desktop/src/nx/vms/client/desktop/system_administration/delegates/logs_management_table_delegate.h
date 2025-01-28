// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtWidgets/QStyledItemDelegate>

#include <nx/vms/client/desktop/system_administration/widgets/logs_management_widget.h>
#include <ui/common/text_pixmap_cache.h>

class QSvgRenderer;
namespace nx::vms::client::desktop {
class LoadingIndicator;

class LogsManagementTableDelegate: public QStyledItemDelegate
{
    using base_type = QStyledItemDelegate;

public:
    explicit LogsManagementTableDelegate(LogsManagementWidget* parent = nullptr);

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

    virtual QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    void drawText(const QModelIndex& index, QRect textRect, QStyle* style, QStyleOptionViewItem option, QPainter* painter, bool extra) const;

protected:
    class LogsManagementLoadingIndicator;

private:
    mutable QnTextPixmapCache m_textPixmapCache;
    LogsManagementWidget* parentWidget = nullptr;
    QScopedPointer<LoadingIndicator> m_loadingIndicator;
};

} // namespace nx::vms::client::desktop
