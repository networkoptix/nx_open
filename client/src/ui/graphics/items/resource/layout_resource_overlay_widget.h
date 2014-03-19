#ifndef LAYOUT_RESOURCE_OVERLAY_WIDGET_H
#define LAYOUT_RESOURCE_OVERLAY_WIDGET_H

#include <core/resource/resource_fwd.h>
#include <core/resource/videowall_item.h>
#include <core/resource/layout_item_data.h>

#include <ui/graphics/items/standard/graphics_widget.h>

#include <utils/common/connective.h>

class QnVideowallResourceScreenWidget;

class QnLayoutResourceOverlayWidget : public Connective<GraphicsWidget> {
    typedef Connective<GraphicsWidget> base_type;
    Q_OBJECT

public:
    explicit QnLayoutResourceOverlayWidget(const QnVideoWallResourcePtr &videowall, const QUuid &itemUuid, QnVideowallResourceScreenWidget *parent, Qt::WindowFlags windowFlags = 0);

protected:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    void at_videoWall_itemChanged(const QnVideoWallResourcePtr &videoWall, const QnVideoWallItem &item);
    void updateLayout();
    void updateView();
    void updateInfo();

    void paintItem(QPainter *painter, const QRectF &paintRect, const QnLayoutItemData &data);

private:
    friend class QnVideowallResourceScreenWidget;

    QnVideowallResourceScreenWidget* m_widget;

    const QnVideoWallResourcePtr m_videowall;
    const QUuid m_itemUuid;

    QnLayoutResourcePtr m_layout;
};

#endif // LAYOUT_RESOURCE_OVERLAY_WIDGET_H
