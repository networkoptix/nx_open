#include "time_scroll_bar.h"

#include <ui/style/noptix_style.h>

QnTimeScrollBar::QnTimeScrollBar(QGraphicsItem *parent):
    base_type(parent) 
{
    
}

QnTimeScrollBar::~QnTimeScrollBar() {
    return;
}

void QnTimeScrollBar::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {

}

void QnTimeScrollBar::updateSliderLength() {

}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnTimeScrollBar::resizeEvent(QGraphicsSceneResizeEvent *event) {
    base_type::resizeEvent(event);

    updateSliderLength();
}

void QnTimeScrollBar::sliderChange(SliderChange change) {
    base_type::sliderChange(change);

    if(change == SliderRangeChange || change == SliderStepsChange || change == SliderMappingChange)
        updateSliderLength();
}

