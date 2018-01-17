#pragma once

#include <QtCore/QScopedPointer>

#include <ui/graphics/items/standard/graphics_widget.h>

namespace nx {
namespace client {
namespace desktop {

class AreaSelectOverlayWidget: public GraphicsWidget
{
    Q_OBJECT
    using base_type = GraphicsWidget;

public:
    AreaSelectOverlayWidget(QGraphicsWidget* parent);
    virtual ~AreaSelectOverlayWidget() override;

    bool active() const;
    void setActive(bool value);

    QRectF selectedArea() const; //< Selected area in normalized coordinates.
    void clearSelectedArea();

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

signals:
    void selectedAreaChanged(const QRectF& selectedArea);

protected:
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    virtual void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx
