#ifndef ANIMATEDWIDGET_H
#define ANIMATEDWIDGET_H

#include <QtGui/QGraphicsWidget>

class AnimatedWidgetPrivate;
class AnimatedWidget : public QGraphicsWidget
{
    Q_OBJECT
    Q_PRIVATE_PROPERTY(d, QPointF instantPos READ instantPos WRITE setInstantPos DESIGNABLE false SCRIPTABLE false FINAL)
    Q_PRIVATE_PROPERTY(d, QSizeF instantSize READ instantSize WRITE setInstantSize DESIGNABLE false SCRIPTABLE false FINAL)
    Q_PRIVATE_PROPERTY(d, qreal instantScale READ instantScale WRITE setInstantScale DESIGNABLE false SCRIPTABLE false FINAL)
    Q_PRIVATE_PROPERTY(d, qreal instantRotation READ instantRotation WRITE setInstantRotation DESIGNABLE false SCRIPTABLE false FINAL)
    Q_PRIVATE_PROPERTY(d, qreal instantOpacity READ instantOpacity WRITE setInstantOpacity DESIGNABLE false SCRIPTABLE false FINAL)

public:
    enum QGraphicsWidgetChange {
        ItemSizeChange = ItemPositionChange + 127,
        ItemSizeHasChanged
    };

    AnimatedWidget(QGraphicsItem *parent = 0);
    ~AnimatedWidget();

    bool isInteractive() const;
    void setInteractive(bool allowed);

    void setGeometry(const QRectF &rect);
    QPainterPath opaqueArea() const;

Q_SIGNALS:
    void clicked();
    void doubleClicked();

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value);

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event);
    void wheelEvent(QGraphicsSceneWheelEvent *event);

private:
    Q_DISABLE_COPY(AnimatedWidget)
    AnimatedWidgetPrivate *const d;
};

#endif // ANIMATEDWIDGET_H
