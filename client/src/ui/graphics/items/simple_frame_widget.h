#ifndef QN_SIMPLE_FRAME_WIDGET_H
#define QN_SIMPLE_FRAME_WIDGET_H

#include <QtGui/QGraphicsWidget>

class QnSimpleFrameWidget: public QGraphicsWidget {
    Q_OBJECT;
public:
    QnSimpleFrameWidget(QGraphicsItem *parent = NULL);
    virtual ~QnSimpleFrameWidget();

    qreal frameWidth() const;
    void setFrameWidth(qreal frameWidth);

    QColor frameColor() const;
    void setFrameColor(const QColor &frameColor);

protected:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    qreal m_frameWidth;
    QColor m_frameColor;
};

#endif // QN_SIMPLE_FRAME_WIDGET_H
