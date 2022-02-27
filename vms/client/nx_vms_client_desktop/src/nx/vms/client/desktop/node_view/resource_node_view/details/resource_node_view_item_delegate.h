// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../../details/node/view_node_fwd.h"
#include "../../node_view/node_view_item_delegate.h"

#include <QtCore/QScopedPointer>

class QnTextPixmapCache;

namespace nx::vms::client::desktop {
namespace node_view {

class ResourceNodeViewItemDelegate: public NodeViewItemDelegate
{
    Q_OBJECT
    using base_type = NodeViewItemDelegate;

public:
    ResourceNodeViewItemDelegate(QObject* parent = nullptr);
    virtual ~ResourceNodeViewItemDelegate() override;

    virtual void paint(
        QPainter* painter,
        const QStyleOptionViewItem& styleOption,
        const QModelIndex& index) const override;

protected:
    virtual void initStyleOption(
        QStyleOptionViewItem* option,
        const QModelIndex& index) const override;

    void paintItemText(
        QPainter* painter,
        QStyleOptionViewItem& option,
        const QModelIndex& index,
        const QColor& mainColor,
        const QColor& extraColor) const;

    void paintItemIcon(
        QPainter* painter,
        const QStyleOptionViewItem& option,
        const QModelIndex& index,
        QIcon::Mode mode) const;

private:
    const QScopedPointer<QnTextPixmapCache> m_textPixmapCache;
};

} // namespace node_view
} // namespace nx::vms::client::desktop
