#pragma once

#include <QtCore/QScopedPointer>

#include <nx/client/desktop/resource_views/node_view/node/view_node_fwd.h>
#include <nx/client/desktop/resource_views/node_view/details/node_view_item_delegate.h>

class QnResourceItemColors;

namespace nx {
namespace client {
namespace desktop {

class ResourceNodeViewItemDelegate: public details::NodeViewItemDelegate
{
    Q_OBJECT
    using base_type = details::NodeViewItemDelegate;

public:
    ResourceNodeViewItemDelegate(
        QTreeView* owner,
        const ColumnsSet& selectionColumns,
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

} // namespace desktop
} // namespace client
} // namespace nx
