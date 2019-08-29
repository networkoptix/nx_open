#pragma once

#include <QtCore/QRect>

namespace nx::analytics::db {

QRect translateToSearchGrid(const QRectF& box);

bool rectsIntersectToSearchPrecision(const QRectF& one, const QRectF& two);

} // namespace nx::analytics::db
