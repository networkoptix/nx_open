// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QColor>

NX_VMS_COMMON_API QColor addColor(const QColor& color, const QColor& shift);
NX_VMS_COMMON_API QColor subColor(const QColor& color, const QColor& shift);

NX_VMS_COMMON_API QColor toGrayscale(const QColor& color);
NX_VMS_COMMON_API QColor toTransparent(const QColor& color);
NX_VMS_COMMON_API QColor toTransparent(const QColor& color, qreal opacity);

NX_VMS_COMMON_API QColor withAlpha(const QColor& color, int alpha);

/**
 * @param background Base color which will be blended with another one. By the meaning, this color
 *     is treated as opaque, alpha channel data won't be taken into account.
 * @param foreground Color which will be blended into base color in a proportion defined by its
 *     opacity.
 * @result Opaque color resulting from linear interpolation of background and foreground colors.
 *     - In case if foreground alpha is 0 it will be the same as background
 *     - In case if foreground alpha is 255 it will be the same as foreground.
 */
NX_VMS_COMMON_API QColor alphaBlend(const QColor& background, const QColor& foreground);
