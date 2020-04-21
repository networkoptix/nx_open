#pragma once

#include <QtCore/QRect>

#include "config.h"

namespace nx::analytics::db {

static constexpr QRect kTrackSearchGrid =
    QRect(0, 0, kTrackSearchResolutionX, kTrackSearchResolutionY);

QRect translateToSearchGrid(const QRectF& box);

bool rectsIntersectToSearchPrecision(const QRectF& one, const QRectF& two);

} // namespace nx::analytics::db
