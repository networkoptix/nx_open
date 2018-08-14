#pragma once

#include "../details/node/view_node_fwd.h"
#include "../node_view/node_view_item_delegate.h"

#include <QtCore/QScopedPointer>

class QnResourceItemColors;

namespace nx {
namespace client {
namespace desktop {
namespace node_view {

class ResourceNodeViewItemDelegate: public NodeViewItemDelegate
{
    Q_OBJECT
    using base_type = NodeViewItemDelegate;

public:
    ResourceNodeViewItemDelegate(
        QTreeView* owner,
        const details::ColumnSet& selectionColumns,
        QObject* parent = nullptr);
    virtual ~ResourceNodeViewItemDelegate() override;

    virtual void paint(
        QPainter *painter,
        const QStyleOptionViewItem &styleOption,
        const QModelIndex &index) const override;

    const QnResourceItemColors& colors() const;
    void setColors(const QnResourceItemColors& colors);

protected:
    virtual void initStyleOption(
        QStyleOptionViewItem* option,
        const QModelIndex& index) const override;

private:
    struct Private;
    const QScopedPointer<Private> d;
};

} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx
