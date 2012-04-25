#ifndef QN_TIME_SCROLL_BAR_H
#define QN_TIME_SCROLL_BAR_H

#include <ui/graphics/items/standard/graphics_scroll_bar.h>

class QnTimeScrollBar: public GraphicsScrollBar {
    Q_OBJECT;

    typedef GraphicsScrollBar base_type;

public:
    QnTimeScrollBar(QGraphicsItem *parent = NULL);
    virtual ~QnTimeScrollBar();

    qint64 indicatorPosition() const;
    void setIndicatorPosition(qint64 indicatorPosition);

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    qint64 m_indicatorPosition;
};


#endif // QN_TIME_SCROLL_BAR_H
