#ifndef QN_CAMERA_BOOKMARK_H
#define QN_CAMERA_BOOKMARK_H

#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QStringList>

#include <common/common_globals.h>

#include <utils/common/uuid.h>

#include "camera_bookmark_fwd.h"

/**
 * @brief The QnCameraBookmark struct               Bookmarked part of the camera archive.
 */
struct QnCameraBookmark {
    /** Unique id. */
    QnUuid guid;

    /** Name of the bookmark.*/
    QString name;

    /** Description of the bookmark. */
    QString description;

    /** Time during which recorded period should be preserved, in milliseconds. */
    qint64 timeout;

    /** Start time in milliseconds since epoch. */
    qint64 startTimeMs;

    /** Duration in milliseconds. */
    qint64 durationMs;

    /** \returns End time in milliseconds since epoch. */
    qint64 endTimeMs() const;

    /** \returns True if bookmark is null, false otherwise. */
    bool isNull() const;

    /** List of tags attached to the bookmark. */
    QnCameraBookmarkTags tags;

    /** Camera unique identifier. */
    QString cameraId;

    QnCameraBookmark():
        timeout(-1),
        startTimeMs(0),
        durationMs(0)
    {}

    QString tagsAsString() const;

    static QnCameraBookmarkList mergeCameraBookmarks(const MultiServerCameraBookmarkList &source, int limit = std::numeric_limits<int>().max(), Qn::BookmarkSearchStrategy strategy = Qn::EarliestFirst);
};
#define QnCameraBookmark_Fields (guid)(name)(description)(timeout)(startTimeMs)(durationMs)(tags)

/**
 * @brief The QnCameraBookmarkSearchFilter struct   Bookmarks search request parameters. Used for loading bookmarks for the fixed time period
 *                                                  with length exceeding fixed minimal, with name and/or tags containing fixed string.
 */
struct QnCameraBookmarkSearchFilter {
    /** Minimum start time for the bookmark. */
    qint64 startTimeMs;

    /** Maximum end time for the bookmark. */
    qint64 endTimeMs;

    /** Text-search filter string. */
    QString text;

    int limit;

    Qn::BookmarkSearchStrategy strategy;

    QnCameraBookmarkSearchFilter();
};
#define QnCameraBookmarkSearchFilter_Fields (startTimeMs)(endTimeMs)(text)(limit)(strategy)

bool operator<(const QnCameraBookmark &first, const QnCameraBookmark &other);

QDebug operator<<(QDebug dbg, const QnCameraBookmark &bookmark);

Q_DECLARE_METATYPE(QnCameraBookmark)
Q_DECLARE_TYPEINFO(QnCameraBookmark, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(QnCameraBookmarkList)
Q_DECLARE_METATYPE(QnCameraBookmarkTags)

#endif //QN_CAMERA_BOOKMARK_H
