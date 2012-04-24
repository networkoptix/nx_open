#ifndef ABSTRACTLINEARGRAPHICSSLIDER_P_H
#define ABSTRACTLINEARGRAPHICSSLIDER_P_H

#include "abstract_graphics_slider_p.h"

class AbstractLinearGraphicsSliderPrivate: public AbstractGraphicsSliderPrivate {
    Q_DECLARE_PUBLIC(AbstractLinearGraphicsSlider);

public:
    void init();

    void invalidateMapper();

    mutable bool mapperDirty;
    mutable bool upsideDown;
    mutable qreal pixelPosMin, pixelPosMax;
    void ensureMapper() const;

};

#endif // ABSTRACTLINEARGRAPHICSSLIDER_P_H
