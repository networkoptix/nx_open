#include "abstract_linear_graphics_slider.h"
#include "abstract_linear_graphics_slider_p.h"

#include <QtWidgets/QStyleOptionSlider>

void AbstractLinearGraphicsSliderPrivate::init()
{
    mapperDirty = true;

    control = QStyle::CC_Slider;
    grooveSubControl = QStyle::SC_SliderGroove;
    handleSubControl = QStyle::SC_SliderHandle;
}

void AbstractLinearGraphicsSliderPrivate::initControls(QStyle::ComplexControl control, QStyle::SubControl grooveSubControl, QStyle::SubControl handleSubControl) {
    this->control = control;
    this->grooveSubControl = grooveSubControl;
    this->handleSubControl = handleSubControl;
}

void AbstractLinearGraphicsSliderPrivate::invalidateMapper() {
    if(mapperDirty)
        return;

    mapperDirty = true;
    q_func()->sliderChange(AbstractLinearGraphicsSlider::SliderMappingChange);
}

void AbstractLinearGraphicsSliderPrivate::ensureMapper() const {
    if(!mapperDirty)
        return;

    Q_Q(const AbstractLinearGraphicsSlider);

    QStyleOptionSlider opt;
    q->initStyleOption(&opt);
    upsideDown = opt.upsideDown;

    QRect grooveRect = q->style()->subControlRect(control, &opt, grooveSubControl, NULL);
    QRect handleRect = q->style()->subControlRect(control, &opt, handleSubControl, NULL);

    if (q->orientation() == Qt::Horizontal) {
        pixelPosMin = grooveRect.x();
        pixelPosMax = grooveRect.right() - handleRect.width() + 1;
    } else {
        pixelPosMin = grooveRect.y();
        pixelPosMax = grooveRect.bottom() - handleRect.height() + 1;
    }

    mapperDirty = false;
}

AbstractLinearGraphicsSlider::AbstractLinearGraphicsSlider(QGraphicsItem *parent):
    base_type(*new AbstractLinearGraphicsSliderPrivate(), parent) 
{
    d_func()->init();
}
    
AbstractLinearGraphicsSlider::AbstractLinearGraphicsSlider(AbstractLinearGraphicsSliderPrivate &dd, QGraphicsItem *parent):
    base_type(dd, parent)
{
    d_func()->init();
}

AbstractLinearGraphicsSlider::~AbstractLinearGraphicsSlider() {
    return;
}

void AbstractLinearGraphicsSlider::updateGeometry() {
    d_func()->invalidateMapper();

    base_type::updateGeometry();
}

void AbstractLinearGraphicsSlider::resizeEvent(QGraphicsSceneResizeEvent *event) {
    d_func()->invalidateMapper();    

    base_type::resizeEvent(event);
}

void AbstractLinearGraphicsSlider::sliderChange(SliderChange change) {
    if(change != SliderValueChange && change != SliderMappingChange)
        d_func()->invalidateMapper();

    base_type::sliderChange(change);
}

bool AbstractLinearGraphicsSlider::event(QEvent *event) {
    if(event->type() == QEvent::StyleChange)
        d_func()->invalidateMapper();

    return base_type::event(event);
}


QPointF AbstractLinearGraphicsSlider::positionFromValue(qint64 logicalValue, bool bound) const {
    Q_D(const AbstractLinearGraphicsSlider);

    d->ensureMapper();

    qreal result = d->pixelPosMin + GraphicsStyle::sliderPositionFromValue(d->minimum, d->maximum, logicalValue, d->pixelPosMax - d->pixelPosMin, d->upsideDown, bound);
    if(d->orientation == Qt::Horizontal) {
        return QPointF(result, 0.0);
    } else {
        return QPointF(0.0, result);
    }
}

qint64 AbstractLinearGraphicsSlider::valueFromPosition(const QPointF &position, bool bound) const {
    Q_D(const AbstractLinearGraphicsSlider);

    d->ensureMapper();

    qreal pixelPos = d->orientation == Qt::Horizontal ? position.x() : position.y();
    return GraphicsStyle::sliderValueFromPosition(d->minimum, d->maximum, pixelPos - d->pixelPosMin, d->pixelPosMax - d->pixelPosMin, d->upsideDown, bound);
}

