#pragma once

#include <QtWidgets/QStyledItemDelegate>

#include <client/client_color_types.h>

namespace nx::vms::client::desktop {
namespace node_view {

class NodeViewItemDelegate: public QStyledItemDelegate
{
    Q_OBJECT
    using base_type = QStyledItemDelegate;

    Q_PROPERTY(NodeViewStatsColors colors READ colors WRITE setColors)

public:
    NodeViewItemDelegate(QObject* parent = nullptr);

    virtual void paint(
        QPainter* painter,
        const QStyleOptionViewItem& styleOption,
        const QModelIndex& index) const override;

    const NodeViewStatsColors& colors() const;
    void setColors(const NodeViewStatsColors& value);

protected:
    virtual void initStyleOption(
        QStyleOptionViewItem* option,
        const QModelIndex& index) const override;

private:
    NodeViewStatsColors m_colors;
};

} // namespace node_view
} // namespace nx::vms::client::desktop

