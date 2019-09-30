#include "analytics_db_utils.h"

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
