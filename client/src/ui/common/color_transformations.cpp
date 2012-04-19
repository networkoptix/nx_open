#include "color_transformations.h"

QColor shiftColor(const QColor &color, int deltaR, int deltaG, int deltaB, int deltaA) {
    return QColor(qMax(0,qMin(255,color.red()+deltaR)), qMax(0,qMin(255,color.green()+deltaG)), qMax(0,qMin(255,color.blue()+deltaR)), qMax(0,qMin(255,color.alpha() + deltaA)));
}

QColor addColor(const QColor &color, const QColor &shift) {
    return shiftColor(color, shift.red(), shift.green(), shift.blue(), shift.alpha());
}

QColor subColor(const QColor &color, const QColor &shift) {
    return shiftColor(color, -shift.red(), -shift.green(), -shift.blue(), -shift.alpha());
}

QColor toGrayscale(const QColor &color) {
    float y  = (0.257 * color.red()) + (0.504 * color.green()) + (0.098 * color.blue()) + 16;
    int v = qMax(0, qMin(255, int (1.164 * (y - 32))));

    return QColor(v, v, v, color.alpha());
}

QColor toTransparent(const QColor &color) {
    QColor result = color;
    result.setAlpha(0);
    return result;
}

QColor toTransparent(const QColor &color, qreal opacity) {
    QColor result = color;
    result.setAlpha(result.alpha() * opacity);
    return result;
}