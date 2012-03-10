#include "color_transform.h"

void toGrayColor(QColor& color)
{
    float Y  = (0.257 * color.red()) + (0.504 * color.green()) + (0.098 * color.blue()) + 16;

    int v = qMax(0, qMin(255, int (1.164*(Y - 32))));
    color.setBlue(v);
    color.setGreen(v);
    color.setRed(v);
}

QColor shiftColor(const QColor& color, int deltaR, int deltaG, int deltaB, int alpha)
{
    return QColor(qMax(0,qMin(255,color.red()+deltaR)), qMax(0,qMin(255,color.green()+deltaG)), qMax(0,qMin(255,color.blue()+deltaR)), qMax(0,qMin(255,color.alpha() + alpha)));
}

QColor addColor(const QColor& color, const QColor& shift)
{
    return shiftColor(color, shift.red(), shift.green(), shift.blue(), shift.alpha());
}

QColor subColor(const QColor& color, const QColor& shift)
{
    return shiftColor(color, -shift.red(), -shift.green(), -shift.blue(), -shift.alpha());
}
