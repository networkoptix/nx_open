#include "ec2_bookmark_data.h"

namespace ec2 {

void fromApiToBookmark(const ApiCameraBookmarkData &data, QnCameraBookmark& bookmark) {
    bookmark.guid = data.guid;
    bookmark.startTimeMs = data.startTime;
    bookmark.durationMs = data.duration;
    bookmark.name = data.name;
    bookmark.description = data.description;
    bookmark.timeout = data.timeout;
    bookmark.tags.clear();
    for (const QString &tag: data.tags)
        bookmark.tags << tag;
}

void fromBookmarkToApi(const QnCameraBookmark &bookmark, ApiCameraBookmarkData &data) {
    data.guid = bookmark.guid.toByteArray();
    data.startTime = bookmark.startTimeMs;
    data.duration = bookmark.durationMs;
    data.name = bookmark.name;
    data.description = bookmark.description;
    data.timeout = bookmark.timeout;
    data.tags.clear();
    data.tags.reserve(bookmark.tags.size());
    for (const QString &tag: bookmark.tags)
        data.tags.push_back(tag.trimmed().toLower());
}

}