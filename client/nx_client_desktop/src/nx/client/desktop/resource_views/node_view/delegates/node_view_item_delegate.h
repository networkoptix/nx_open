#pragma once

#include <QtWidgets/QStyledItemDelegate>

class QTreeView;
class QPainter;

namespace nx {
namespace client {
namespace desktop {

class NodeViewItemDelegate: public QStyledItemDelegate
{
    Q_OBJECT
    using base_type = QStyledItemDelegate;

public:
    NodeViewItemDelegate(QTreeView* owner, QObject* parent);

    const QTreeView* owner() const;

protected:
    virtual void initStyleOption(
        QStyleOptionViewItem* option,
        const QModelIndex& index) const override;

private:
    QTreeView const * const m_owner;
};

} // namespace desktop
} // namespace client
} // namespace nx
