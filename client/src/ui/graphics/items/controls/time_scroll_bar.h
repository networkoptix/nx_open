#ifndef QN_TIME_SCROLL_BAR_H
#define QN_TIME_SCROLL_BAR_H

#include <ui/graphics/items/standard/graphics_scroll_bar.h>

class QnTimeScrollBar: public GraphicsScrollBar {
    Q_OBJECT;

    typedef GraphicsScrollBar base_type;

public:
    QnTimeScrollBar(QGraphicsItem *parent = NULL);
    virtual ~QnTimeScrollBar();

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

protected:
    virtual void sliderChange(SliderChange change) override;

    virtual void resizeEvent(QGraphicsSceneResizeEvent *event) override;

private:
    void updateSliderLength();
};


#endif // QN_TIME_SCROLL_BAR_H
