#include "camera_bookmark.h"

#include <utils/common/model_functions.h>

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

void serialize_field(const QStringList& /*value*/, QVariant* /*target*/) {return ;}
void deserialize_field(const QVariant& /*value*/, QStringList* /*target*/) {return ;}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnCameraBookmark, (sql)(json), (startTimeMs)(durationMs)(guid)(name)(description)(timeout)(tags) )
