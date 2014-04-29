#include "camera_bookmark.h"

#include <utils/math/defines.h>

#include <utils/common/model_functions.h>

qint64 QnCameraBookmark::endTimeMs() const {
    return startTimeMs + durationMs;
}


QDebug operator<<(QDebug dbg, const QnCameraBookmark &bookmark) {
    if (bookmark.durationMs > 0)
        dbg.nospace() << "QnCameraBookmark(" << QDateTime::fromMSecsSinceEpoch(bookmark.startTimeMs).toString(lit("dd hh:mm"))
        << " - " << QDateTime::fromMSecsSinceEpoch(bookmark.startTimeMs + bookmark.durationMs).toString(lit("dd hh:mm")) << ')';
    else
        dbg.nospace() << "QnCameraBookmark INSTANT (" << QDateTime::fromMSecsSinceEpoch(bookmark.startTimeMs).toString(lit("dd hh:mm")) << ')';
    dbg.space() << "timeout" << bookmark.timeout;
    dbg.space() << bookmark.name << bookmark.description;
    dbg.space() << bookmark.tags.join(lit(", "));
    return dbg.space();
}

QnCameraBookmarkSearchFilter::QnCameraBookmarkSearchFilter():
    minStartTimeMs(0),
    maxStartTimeMs(INT64_MAX),
    minDurationMs(0)
{}

void serialize_field(const QStringList& /*value*/, QVariant* /*target*/) {return ;}
void deserialize_field(const QVariant& /*value*/, QStringList* /*target*/) {return ;}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnCameraBookmark, (sql)(json)(eq), (startTimeMs)(durationMs)(guid)(name)(description)(timeout)(tags) )
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnCameraBookmarkSearchFilter, (json)(eq), (minStartTimeMs)(maxStartTimeMs)(minDurationMs) )
