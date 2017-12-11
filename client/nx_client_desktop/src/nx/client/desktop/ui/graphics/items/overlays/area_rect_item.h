#pragma once

#include <nx/client/desktop/ui/graphics/items/graphics_rect_item.h>

namespace nx {
namespace client {
namespace desktop {

class AreaRectItem: public QObject, public GraphicsRectItem
{
    Q_OBJECT

public:
    AreaRectItem(QGraphicsItem* parentItem = nullptr, QObject* parentObject = nullptr);

    bool containsMouse() const;

signals:
    void containsMouseChanged(bool containsMouse);
    void clicked();

protected:
    virtual void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
    virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;
    virtual void mousePressEvent(QGraphicsSceneMouseEvent* event) override;

private:
    bool m_containsMouse = false;
};

} // namespace desktop
} // namespace client
} // namespace nx
