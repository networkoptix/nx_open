// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "color_transformations.h"

namespace {

QColor shiftColor(const QColor& color, int deltaR, int deltaG, int deltaB, int deltaA)
{
    return QColor(
        std::clamp(color.red() + deltaR, 0, 0xFF),
        std::clamp(color.green() + deltaG, 0, 0xFF),
        std::clamp(color.blue() + deltaB, 0, 0xFF),
        std::clamp(color.alpha() + deltaA, 0, 0xFF));
}

} // namespace

QColor addColor(const QColor& color, const QColor& shift)
{
    return shiftColor(color, shift.red(), shift.green(), shift.blue(), shift.alpha());
}

QColor subColor(const QColor& color, const QColor& shift)
{
    return shiftColor(color, -shift.red(), -shift.green(), -shift.blue(), -shift.alpha());
}

QColor toGrayscale(const QColor& color)
{
    float y = (0.257 * color.red()) + (0.504 * color.green()) + (0.098 * color.blue()) + 16;
    int v = std::clamp(static_cast<int>(1.164 * (y - 32)), 0, 0xFF);

    return QColor(v, v, v, color.alpha());
}

QColor toTransparent(const QColor& color)
{
    QColor result = color;
    result.setAlpha(0);
    return result;
}

QColor toTransparent(const QColor& color, qreal opacity)
{
    QColor result = color;
    result.setAlpha(result.alpha() * opacity);
    return result;
}

QColor withAlpha(const QColor& color, int alpha)
{
    QColor result = color;
    result.setAlpha(alpha);
    return result;
}

QColor alphaBlend(const QColor& background, const QColor& foreground)
{
    const int alpha = foreground.alpha() + 1;
    const int invertedAlpha = 0xFF - foreground.alpha();

    return QColor(
        (foreground.red() * alpha + background.red() * invertedAlpha) >> 8,
        (foreground.green() * alpha + background.green() * invertedAlpha) >> 8,
        (foreground.blue() * alpha + background.blue() * invertedAlpha) >> 8);
}
