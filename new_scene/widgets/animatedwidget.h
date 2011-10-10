#ifndef ANIMATEDWIDGET_H
#define ANIMATEDWIDGET_H

#include "graphicswidget.h"

class QAnimationGroup;

class AnimatedWidgetPrivate;
class AnimatedWidget : public GraphicsWidget
{
    Q_OBJECT
    Q_PRIVATE_PROPERTY(d_func(), QPointF instantPos READ instantPos WRITE setInstantPos DESIGNABLE false SCRIPTABLE false FINAL)
    Q_PRIVATE_PROPERTY(d_func(), QSizeF instantSize READ instantSize WRITE setInstantSize DESIGNABLE false SCRIPTABLE false FINAL)
    Q_PRIVATE_PROPERTY(d_func(), float instantScale READ instantScale WRITE setInstantScale DESIGNABLE false SCRIPTABLE false FINAL)
    Q_PRIVATE_PROPERTY(d_func(), float instantRotation READ instantRotation WRITE setInstantRotation DESIGNABLE false SCRIPTABLE false FINAL)
    Q_PRIVATE_PROPERTY(d_func(), float instantOpacity READ instantOpacity WRITE setInstantOpacity DESIGNABLE false SCRIPTABLE false FINAL)

    typedef GraphicsWidget base_type;

public:
    enum QGraphicsWidgetChange {
        ItemSizeChange = ItemPositionChange + 127,
        ItemSizeHasChanged
    };

    AnimatedWidget(QGraphicsItem *parent = 0);
    ~AnimatedWidget();

    bool isInteractive() const;
    void setInteractive(bool interactive);

    QAnimationGroup *animationGroup() const;

    QPainterPath opaqueArea() const;

    void setGeometry(const QRectF &rect);

protected:
    void updateGeometry();

    QVariant itemChange(GraphicsItemChange change, const QVariant &value);

    bool windowFrameEvent(QEvent *event);
    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event);
    void wheelEvent(QGraphicsSceneWheelEvent *event);

private:
    Q_DECLARE_PRIVATE(AnimatedWidget);
};

#endif // ANIMATEDWIDGET_H
