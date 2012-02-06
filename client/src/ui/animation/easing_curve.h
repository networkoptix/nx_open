#ifndef QN_EASING_CURVE_H
#define QN_EASING_CURVE_H

#include <QEasingCurve>

enum QnEasingCurve
{
    SLOW_END,
    SLOW_END_POW_20,
    SLOW_END_POW_22,
    SLOW_END_POW_25,
    SLOW_END_POW_30,
    SLOW_END_POW_35,
    SLOW_END_POW_40,
    SLOW_START_SLOW_END,
    INOUTBACK,
    OUTCUBIC,
    SLOW_START 
};

QEasingCurve easingCurve(QnEasingCurve curve);

#endif // QN_EASING_CURVE_H
