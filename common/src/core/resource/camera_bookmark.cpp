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

inline void serialize_field(const QStringList &, QVariant *) { return; }
inline void deserialize_field(const QVariant &, QStringList *) { return; }


QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnCameraBookmark, (sql), (guid)(name)(description)(timeout) )