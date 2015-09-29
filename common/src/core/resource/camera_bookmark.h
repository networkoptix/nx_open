#ifndef QN_CAMERA_BOOKMARK_H
#define QN_CAMERA_BOOKMARK_H

#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QStringList>

#include <common/common_globals.h>

#include <utils/common/uuid.h>

#include "camera_bookmark_fwd.h"

#include <utils/common/model_functions_fwd.h>

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

    QString tagsAsString(QChar delimiter = L' ') const;

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
    qint64 endTimeMs; //TODO: #GDM #Bookmarks now works as maximum start time

    /** Text-search filter string. */
    QString text;

    int limit; //TODO: #GDM #Bookmarks works in merge function only

    Qn::BookmarkSearchStrategy strategy; //TODO: #GDM #Bookmarks works in merge function only

    QnCameraBookmarkSearchFilter();

    bool isValid() const;
};
#define QnCameraBookmarkSearchFilter_Fields (startTimeMs)(endTimeMs)(text)(limit)(strategy)

bool operator<(const QnCameraBookmark &first, const QnCameraBookmark &other);
bool operator<(qint64 first, const QnCameraBookmark &other);
bool operator<(const QnCameraBookmark &first, qint64 other);

QDebug operator<<(QDebug dbg, const QnCameraBookmark &bookmark);

Q_DECLARE_TYPEINFO(QnCameraBookmark, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(QnCameraBookmarkList)
Q_DECLARE_METATYPE(QnCameraBookmarkTags)

QN_FUSION_DECLARE_FUNCTIONS(QnCameraBookmark, (sql_record)(json)(ubjson)(xml)(csv_record)(metatype)(eq))
QN_FUSION_DECLARE_FUNCTIONS(QnCameraBookmarkSearchFilter, (json)(metatype)(eq))

#endif //QN_CAMERA_BOOKMARK_H
