#ifndef QN_SIMPLE_FRAME_WIDGET_H
#define QN_SIMPLE_FRAME_WIDGET_H

#include <ui/graphics/items/standard/graphics_widget.h>

/**
 * A graphics frame widget that does not use style for painting.
 * 
 * Frame width and color are configurable.
 */
class QnSimpleFrameWidget: public GraphicsWidget {
    Q_OBJECT;

    typedef GraphicsWidget base_type;

public:
    QnSimpleFrameWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0);
    virtual ~QnSimpleFrameWidget();

    qreal frameWidth() const;
    void setFrameWidth(qreal frameWidth);

    QBrush frameBrush() const;
    void setFrameBrush(const QBrush &frameBrush);

    QColor frameColor() const;
    void setFrameColor(const QColor &frameColor);

    QBrush windowBrush() const;
    void setWindowBrush(const QBrush &windowBrush);

    QColor windowColor() const;
    void setWindowColor(const QColor &windowColor);

protected:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    qreal m_frameWidth;
};

#endif // QN_SIMPLE_FRAME_WIDGET_H
