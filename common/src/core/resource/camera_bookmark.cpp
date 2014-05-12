#include "camera_bookmark.h"

#include <utils/math/defines.h>

#include <utils/common/model_functions.h>

qint64 QnCameraBookmark::endTimeMs() const {
    return startTimeMs + durationMs;
}

bool QnCameraBookmark::isNull() const {
    return guid.isNull();
}

bool operator<(const QnCameraBookmark &first, const QnCameraBookmark &other) {
    if (first.startTimeMs == other.startTimeMs)
        return first.guid.toRfc4122() < other.guid.toRfc4122();
    return first.startTimeMs < other.startTimeMs;
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

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnCameraBookmark, (sql_record)(json)(eq), QnCameraBookmark_Fields, (optional, true) )
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnCameraBookmarkSearchFilter, (json)(eq), QnCameraBookmarkSearchFilter_Fields, (optional, true) )
