#ifndef ABSTRACTLINEARGRAPHICSSLIDER_H
#define ABSTRACTLINEARGRAPHICSSLIDER_H

#include "abstract_graphics_slider.h"

class AbstractLinearGraphicsSliderPrivate;

class AbstractLinearGraphicsSlider: public AbstractGraphicsSlider {
    Q_OBJECT;

    typedef AbstractGraphicsSlider base_type;

public:
    explicit AbstractLinearGraphicsSlider(QGraphicsItem *parent = 0);
    virtual ~AbstractLinearGraphicsSlider();

    virtual QPointF positionFromValue(qint64 logicalValue, bool bound = true) const override;
    virtual qint64 valueFromPosition(const QPointF &position, bool bound = true) const override;

    virtual void updateGeometry() override;

protected:
    AbstractLinearGraphicsSlider(AbstractLinearGraphicsSliderPrivate &dd, QGraphicsItem *parent);

    virtual bool event(QEvent *event) override;
    virtual void resizeEvent(QGraphicsSceneResizeEvent *event) override;

    virtual void sliderChange(SliderChange change) override;
    
private:
    Q_DECLARE_PRIVATE(AbstractLinearGraphicsSlider);
};



#endif // ABSTRACTLINEARGRAPHICSSLIDER_H
