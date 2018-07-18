#pragma once

#include <QtWidgets/QStyledItemDelegate>

class QTreeView;

namespace nx {
namespace client {
namespace desktop {
namespace details {

class NodeViewItemDelegate: public QStyledItemDelegate
{
    Q_OBJECT
    using base_type = QStyledItemDelegate;

public:
    NodeViewItemDelegate(QTreeView* owner, QObject* parent = nullptr);

    QTreeView* owner() const;

    virtual void paint(
        QPainter *painter,
        const QStyleOptionViewItem &styleOption,
        const QModelIndex &index) const override;

protected:
    virtual void initStyleOption(
        QStyleOptionViewItem* option,
        const QModelIndex& index) const override;

private:
    QTreeView* const m_owner;
};

} // namespace details
} // namespace desktop
} // namespace client
} // namespace nx
