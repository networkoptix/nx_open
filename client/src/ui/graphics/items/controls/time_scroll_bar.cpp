#include "time_scroll_bar.h"

#include <ui/style/noptix_style.h>

QnTimeScrollBar::QnTimeScrollBar(QGraphicsItem *parent):
    base_type(Qt::Horizontal, parent) 
{
    
}

QnTimeScrollBar::~QnTimeScrollBar() {
    return;
}

void QnTimeScrollBar::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    QStyleOptionSlider opt;
    initStyleOption(&opt);

    QRect sliderRect = style()->subControlRect(QStyle::CC_ScrollBar, &opt, QStyle::SC_ScrollBarSlider, this);

    painter->setPen(QPen(Qt::green, 0));
    painter->drawRect(sliderRect);
}


