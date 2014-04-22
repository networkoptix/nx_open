#include "ec2_bookmark_data.h"

namespace ec2 {

void ApiCameraBookmark::toBookmark(QnCameraBookmark& bookmark) const {
    bookmark.guid = guid;
    bookmark.startTimeMs = startTime;
    bookmark.durationMs = duration;
    bookmark.name = name;
    bookmark.description = description;
    bookmark.timeout = timeout;
    bookmark.tags.clear();
    for (const QString &tag: tags)
        bookmark.tags << tag;
}

void ApiCameraBookmark::fromBookmark(const QnCameraBookmark& bookmark) {
    guid = bookmark.guid.toByteArray();
    startTime = bookmark.startTimeMs;
    duration = bookmark.durationMs;
    name = bookmark.name;
    description = bookmark.description;
    timeout = bookmark.timeout;
    tags.clear();
    tags.reserve(bookmark.tags.size());
    for (const QString &tag: bookmark.tags)
        tags.push_back(tag.trimmed());
}

}