#pragma once

#include "../../details/node/view_node_fwd.h"
#include "../../node_view/node_view_item_delegate.h"

#include <QtCore/QScopedPointer>

#include <ui/customization/customized.h>
#include <client/client_color_types.h>

namespace nx::vms::client::desktop {
namespace node_view {

class ResourceNodeViewItemDelegate: public Customized<NodeViewItemDelegate>
{
    Q_OBJECT
    using base_type = Customized<NodeViewItemDelegate>;

    Q_PROPERTY(QnResourceItemColors resourceColors READ resourceColors WRITE setResourceColors)

public:
    ResourceNodeViewItemDelegate(QObject* parent = nullptr);
    virtual ~ResourceNodeViewItemDelegate() override;

    virtual void paint(
        QPainter* painter,
        const QStyleOptionViewItem& styleOption,
        const QModelIndex& index) const override;

    const QnResourceItemColors& resourceColors() const;
    void setResourceColors(const QnResourceItemColors& colors);

    bool getShowRecordingIndicator() const;
    void setShowRecordingIndicator(bool show);

protected:
    virtual void initStyleOption(
        QStyleOptionViewItem* option,
        const QModelIndex& index) const override;

    void paintItemText(
        QPainter* painter,
        QStyleOptionViewItem& option,
        const QModelIndex& index,
        const QColor& mainColor,
        const QColor& extraColor,
        const QColor& invalidColor) const;

    void paintItemIcon(
        QPainter* painter,
        const QStyleOptionViewItem& option,
        const QModelIndex& index,
        QIcon::Mode mode) const;

    void paintRecordingIndicator(QPainter* painter,
        const QRect& iconRect,
        const QModelIndex& index) const;

private:
    struct Private;
    const QScopedPointer<Private> d;
};

} // namespace node_view
} // namespace nx::vms::client::desktop
