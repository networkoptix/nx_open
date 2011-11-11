#ifndef ANIMATEDWIDGET_H
#define ANIMATEDWIDGET_H

#include "graphicswidget.h"

class AnimatedWidgetPrivate;
class AnimatedWidget : public GraphicsWidget
{
    Q_OBJECT
    Q_PROPERTY(bool interactive READ isInteractive WRITE setInteractive FINAL)
    Q_PROPERTY(bool animationsEnabled READ isAnimationsEnabled WRITE setAnimationsEnabled FINAL)
    Q_PROPERTY(QPointF instantPos READ instantPos WRITE setInstantPos DESIGNABLE false SCRIPTABLE false FINAL)
    Q_PROPERTY(QSizeF instantSize READ instantSize WRITE setInstantSize DESIGNABLE false SCRIPTABLE false FINAL)
    Q_PROPERTY(float instantScale READ instantScale WRITE setInstantScale DESIGNABLE false SCRIPTABLE false FINAL)
    Q_PROPERTY(float instantRotation READ instantRotation WRITE setInstantRotation DESIGNABLE false SCRIPTABLE false FINAL)
    Q_PROPERTY(float instantOpacity READ instantOpacity WRITE setInstantOpacity DESIGNABLE false SCRIPTABLE false FINAL)

public:
    AnimatedWidget(QGraphicsItem *parent = 0);
    virtual ~AnimatedWidget();

    bool isInteractive() const;
    void setInteractive(bool interactive);

    bool isAnimationsEnabled() const;
    void setAnimationsEnabled(bool enable);

    QPainterPath opaqueArea() const;

    void setGeometry(const QRectF &rect);

protected:
    void updateGeometry();

    QVariant itemChange(GraphicsItemChange change, const QVariant &value);

protected:
    AnimatedWidget(AnimatedWidgetPrivate &dd, QGraphicsItem *parent);

private:
    QPointF instantPos() const;
    QSizeF instantSize() const;
    qreal instantScale() const;
    qreal instantRotation() const;
    qreal instantOpacity() const;

    void setInstantPos(const QPointF &value);
    void setInstantSize(const QSizeF &value);
    void setInstantScale(qreal value);
    void setInstantRotation(qreal value);
    void setInstantOpacity(qreal value);

private:
    Q_DECLARE_PRIVATE(AnimatedWidget)
    Q_DISABLE_COPY(AnimatedWidget)
};

#endif // ANIMATEDWIDGET_H

