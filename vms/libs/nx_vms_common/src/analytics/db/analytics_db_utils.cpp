// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_db_utils.h"

#include <cmath>

#include <nx/streaming/media_data_packet.h>

namespace nx::analytics::db {

QRect translateToSearchGrid(const QRectF& box)
{
    return QnMetaDataV1::rectFromNormalizedRect(box);
}

bool rectsIntersectToSearchPrecision(const QRectF& one, const QRectF& two)
{
    const auto translatedOne = translateToSearchGrid(one);
    const auto translatedTwo = translateToSearchGrid(two);

    return translatedOne.intersects(translatedTwo);
}

} // namespace nx::analytics::db
