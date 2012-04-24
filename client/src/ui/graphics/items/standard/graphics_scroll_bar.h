#ifndef GRAPHICSSCROLLBAR_H
#define GRAPHICSSCROLLBAR_H

#include "abstract_linear_graphics_slider.h"

class GraphicsScrollBarPrivate;

class GraphicsScrollBar : public AbstractLinearGraphicsSlider {
    Q_OBJECT;

    typedef AbstractLinearGraphicsSlider base_type;

public:
    explicit GraphicsScrollBar(QGraphicsItem *parent = 0);
    explicit GraphicsScrollBar(Qt::Orientation, QGraphicsItem *parent = 0);
    ~GraphicsScrollBar();

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0) override;

    virtual QPointF positionFromValue(qint64 logicalValue) const override;
    virtual qint64 valueFromPosition(const QPointF &position) const override;

protected:
    virtual bool event(QEvent *event) override;
    virtual void mousePressEvent(QGraphicsSceneMouseEvent *) override;
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *) override;
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *) override;
    virtual void hideEvent(QHideEvent *) override;
    virtual void sliderChange(SliderChange change) override;
#ifndef QT_NO_CONTEXTMENU
    virtual void contextMenuEvent(QGraphicsSceneContextMenuEvent *) override;
#endif
    virtual QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint) const override;

    virtual void initStyleOption(QStyleOption *option) const override;

private:
    Q_DISABLE_COPY(GraphicsScrollBar)
    Q_DECLARE_PRIVATE(GraphicsScrollBar)
};

#endif // GRAPHICSSCROLLBAR_H
