#pragma once

#include <QtCore/QScopedPointer>

#include <ui/graphics/items/standard/graphics_widget.h>

class QnResourceWidget;

namespace nx::vms::client::desktop {

class AreaSelectOverlayWidget: public GraphicsWidget
{
    Q_OBJECT
    using base_type = GraphicsWidget;

public:
    AreaSelectOverlayWidget(QGraphicsWidget* parent, QnResourceWidget* resourceWidget);
    virtual ~AreaSelectOverlayWidget() override;

    bool active() const;
    void setActive(bool value);

    QRectF selectedArea() const; //< Selected area in normalized coordinates.
    void setSelectedArea(const QRectF& value);
    void clearSelectedArea();

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

signals:
    void selectionStarted(QPrivateSignal);
    void selectedAreaChanged(const QRectF& selectedArea, QPrivateSignal);

protected:
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    virtual void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
