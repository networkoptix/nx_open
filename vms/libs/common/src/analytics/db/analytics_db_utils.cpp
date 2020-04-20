#include "analytics_db_utils.h"

#include <nx/streaming/media_data_packet.h>

namespace nx::analytics::db {

QRect translateToSearchGrid(const QRectF& box)
{
    auto rect = QnMetaDataV1::rectFromNormalizedRect(box);
    // Making sure rect is not empty.
    if (rect.width() == 0)
        rect.setWidth(1);
    if (rect.height() == 0)
        rect.setHeight(1);
    return rect;
}

bool rectsIntersectToSearchPrecision(const QRectF& one, const QRectF& two)
{
    const auto translatedOne = translateToSearchGrid(one);
    const auto translatedTwo = translateToSearchGrid(two);

    return translatedOne.intersects(translatedTwo);
}

} // namespace nx::analytics::db
