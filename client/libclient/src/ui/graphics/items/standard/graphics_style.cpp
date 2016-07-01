#include "graphics_style.h"

GraphicsStyle::GraphicsStyle() {
    return;
}

GraphicsStyle::~GraphicsStyle() {
    return;
}

void GraphicsStyle::polish(QGraphicsWidget *) {
    return;
}

void GraphicsStyle::unpolish(QGraphicsWidget *) {
    return;
}

qreal GraphicsStyle::sliderPositionFromValue(qint64 min, qint64 max, qint64 logicalValue, qreal span, bool upsideDown, bool bound) {
    if(span <= 0 || max <= min)
        return 0;

    if(bound) {
        if (logicalValue < min)
            return 0;

        if (logicalValue > max)
            return upsideDown ? 0.0 : span;
    }

    qint64 range = max - min;
    qint64 p = upsideDown ? max - logicalValue : logicalValue - min;

    return p / (range / span);
}

qint64 GraphicsStyle::sliderValueFromPosition(qint64 min, qint64 max, qreal pos, qreal span, bool upsideDown, bool bound) {
    if(span <= 0)
        return 0;

    if(bound) {
        if (pos <= 0)
            return upsideDown ? max : min;

        if (pos >= span)
            return upsideDown ? min : max;
    }

    qreal tmp = qRound64((max - min) * (pos / span));
    return upsideDown ? max - tmp : tmp + min;
}



