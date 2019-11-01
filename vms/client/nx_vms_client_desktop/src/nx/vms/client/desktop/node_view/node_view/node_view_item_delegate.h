#pragma once

#include <QtWidgets/QStyledItemDelegate>

namespace nx::vms::client::desktop {
namespace node_view {

class NodeViewItemDelegate: public QStyledItemDelegate
{
    Q_OBJECT
    using base_type = QStyledItemDelegate;

public:
    NodeViewItemDelegate(QObject* parent = nullptr);

    virtual void paint(
        QPainter* painter,
        const QStyleOptionViewItem& styleOption,
        const QModelIndex& index) const override;

protected:
    virtual void initStyleOption(
        QStyleOptionViewItem* option,
        const QModelIndex& index) const override;
};

} // namespace node_view
} // namespace nx::vms::client::desktop

