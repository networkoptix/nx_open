#ifndef QN_CAMERA_BOOKMARK_H
#define QN_CAMERA_BOOKMARK_H

#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QStringList>

#include <common/common_globals.h>

#include <utils/common/uuid.h>

#include "camera_bookmark_fwd.h"

#include <utils/common/model_functions_fwd.h>

struct QnBookmarkSortOrder
{
    Qn::BookmarkSortField column;
    Qt::SortOrder order;

    explicit QnBookmarkSortOrder(Qn::BookmarkSortField column = Qn::BookmarkStartTime
        , Qt::SortOrder order = Qt::AscendingOrder);

    static const QnBookmarkSortOrder defaultOrder;
};
#define QnBookmarkSortOrder_Fields (column)(order)

struct QnBookmarkSparsingOptions
{
    bool used;
    int minVisibleLengthMs;

    explicit QnBookmarkSparsingOptions(bool used = false
        , qint64 minVisibleLengthMs = 0);

    static const QnBookmarkSparsingOptions kNosparsing;
};
#define QnBookmarkSparsingOptions_Fileds (used)(minVisibleLengthMs)

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

    /** List of tags attached to the bookmark. */
    QnCameraBookmarkTags tags;

    /** Camera unique identifier. */
    QString cameraId;

    QnCameraBookmark():
        timeout(-1),
        startTimeMs(0),
        durationMs(0)
    {}

    /** \returns True if bookmark is null, false otherwise. */
    bool isNull() const;

    /** \returns True if bookmark is valid, false otherwise. */
    bool isValid() const;

    static QString tagsToString(const QnCameraBookmarkTags &tags, const QString &delimiter = lit(", "));

    static void sortBookmarks(QnCameraBookmarkList &bookmarks
        , const QnBookmarkSortOrder orderBy);

    static QnCameraBookmarkList mergeCameraBookmarks(const QnMultiServerCameraBookmarkList &source
        , const QnBookmarkSortOrder &sortProperties = QnBookmarkSortOrder::defaultOrder
        , const QnBookmarkSparsingOptions &sparsing = QnBookmarkSparsingOptions()
        , int limit = std::numeric_limits<int>().max());
};
#define QnCameraBookmark_Fields (guid)(name)(description)(timeout)(startTimeMs)(durationMs)(tags)(cameraId)

/**
 * @brief The QnCameraBookmarkSearchFilter struct   Bookmarks search request parameters. Used for loading bookmarks for the fixed time period
 *                                                  with length exceeding fixed minimal, with name and/or tags containing fixed string.
 */

struct QnCameraBookmarkSearchFilter
{
    /** Minimum start time for the bookmark. */
    qint64 startTimeMs;

    /** Maximum end time for the bookmark. */
    qint64 endTimeMs; //TODO: #GDM #Bookmarks now works as maximum start time

    /** Text-search filter string. */
    QString text;

    int limit; //TODO: #GDM #Bookmarks works in merge function only

    QnBookmarkSparsingOptions sparsing;

    QnBookmarkSortOrder orderBy;

    QnCameraBookmarkSearchFilter();

    bool isValid() const;

    bool checkBookmark(const QnCameraBookmark &bookmark) const;

    static QnCameraBookmarkSearchFilter invalidFilter();

    static const int kNoLimit;
};
#define QnCameraBookmarkSearchFilter_Fields (startTimeMs)(endTimeMs)(text)(limit)(orderBy)

struct QnCameraBookmarkTag
{
    QString name;
    int count;

    QnCameraBookmarkTag() :
        count(0)
    {}

    QnCameraBookmarkTag(const QString &initName
        , int initCount) :
        name(initName),
        count(initCount)
    {}

    static QnCameraBookmarkTagList mergeCameraBookmarkTags(const QnMultiServerCameraBookmarkTagList &source, int limit = std::numeric_limits<int>().max());
};
#define QnCameraBookmarkTag_Fields (name)(count)

bool operator<(const QnCameraBookmark &first, const QnCameraBookmark &other);
bool operator<(qint64 first, const QnCameraBookmark &other);
bool operator<(const QnCameraBookmark &first, qint64 other);

QDebug operator<<(QDebug dbg, const QnCameraBookmark &bookmark);

Q_DECLARE_TYPEINFO(QnCameraBookmark, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(QnCameraBookmarkList)
Q_DECLARE_METATYPE(QnCameraBookmarkTags)
Q_DECLARE_METATYPE(QnCameraBookmarkTagList)

QN_FUSION_DECLARE_FUNCTIONS(QnBookmarkSortOrder, (json)(metatype)(eq))
QN_FUSION_DECLARE_FUNCTIONS(QnBookmarkSparsingOptions, (json)(metatype)(eq))
QN_FUSION_DECLARE_FUNCTIONS(QnCameraBookmarkSearchFilter, (json)(metatype)(eq))

QN_FUSION_DECLARE_FUNCTIONS(QnCameraBookmark,    (sql_record)(json)(ubjson)(xml)(csv_record)(metatype)(eq))
QN_FUSION_DECLARE_FUNCTIONS(QnCameraBookmarkTag, (sql_record)(json)(ubjson)(xml)(csv_record)(metatype)(eq))

#endif //QN_CAMERA_BOOKMARK_H
