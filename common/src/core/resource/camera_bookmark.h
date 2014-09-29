#ifndef QN_CAMERA_BOOKMARK_H
#define QN_CAMERA_BOOKMARK_H

#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QStringList>
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

    QnCameraBookmark():
        timeout(-1),
        startTimeMs(0),
        durationMs(0)
    {}
};
#define QnCameraBookmark_Fields (guid)(name)(description)(timeout)(startTimeMs)(durationMs)(tags)

/**
 * @brief The QnCameraBookmarkSearchFilter struct   Bookmarks search request parameters. Used for loading bookmarks for the fixed time period
 *                                                  with length exceeding fixed minimal, with name and/or tags containing fixed string.
 */
struct QnCameraBookmarkSearchFilter {
    //TODO: #GDM #Bookmarks change minStartTimeMs to maxEndTimeMs to load bookmarks that end in the current window.
    /** Minimum start time for the bookmark. */
    qint64 minStartTimeMs;

    /** Maximum start time for the bookmark. */
    qint64 maxStartTimeMs;

    /** Minimum bookmark duration. */
    qint64 minDurationMs;

    /** Text-search filter string. */
    QString text;

    QnCameraBookmarkSearchFilter();
};
#define QnCameraBookmarkSearchFilter_Fields (minStartTimeMs)(maxStartTimeMs)(minDurationMs)(text)

bool operator<(const QnCameraBookmark &first, const QnCameraBookmark &other);

QDebug operator<<(QDebug dbg, const QnCameraBookmark &bookmark);

Q_DECLARE_METATYPE(QnCameraBookmark)
Q_DECLARE_TYPEINFO(QnCameraBookmark, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(QnCameraBookmarkList)
Q_DECLARE_METATYPE(QnCameraBookmarkTags)

#endif //QN_CAMERA_BOOKMARK_H
