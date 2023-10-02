// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <limits>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/reflect/json.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/bookmark_models.h>

#include "camera_bookmark_fwd.h"

namespace nx::vms::common { class SystemContext; }

struct NX_VMS_COMMON_API QnBookmarkSortOrder
{
    nx::vms::api::BookmarkSortField column = nx::vms::api::BookmarkSortField::startTime;
    Qt::SortOrder order = Qt::AscendingOrder;

    explicit QnBookmarkSortOrder(
        nx::vms::api::BookmarkSortField column = nx::vms::api::BookmarkSortField::startTime,
        Qt::SortOrder order = Qt::AscendingOrder);

    bool operator==(const QnBookmarkSortOrder& other) const = default;

    static const QnBookmarkSortOrder defaultOrder;
};
#define QnBookmarkSortOrder_Fields (column)(order)
NX_REFLECTION_INSTRUMENT(QnBookmarkSortOrder, QnBookmarkSortOrder_Fields)

/**
 * Bookmarked part of the camera archive.
 */
struct NX_VMS_COMMON_API QnCameraBookmark
{
    using milliseconds = std::chrono::milliseconds;
    /**%apidoc[readonly] Id of the bookmark. */
    QnUuid guid;

    /**%apidoc[readonly] Identifier of the user who created this bookmark. */
    QnUuid creatorId;

    /**%apidoc[opt] Time of the bookmark creation in milliseconds since epoch. Equals to
      * startTimeMs field if the bookmark is created by a system.
      */
    milliseconds creationTimeStampMs = milliseconds(0); //< Ms in names are left for serialization.

    /**%apidoc Caption of the bookmark. */
    QString name;

    /**%apidoc[opt] Details of the bookmark. */
    QString description;

    /**%apidoc[opt]
     * Time during which a recorded period should be preserved (in milliseconds).
     * %deprecated
     */
    milliseconds timeout = milliseconds(-1);

    /**%apidoc Start time of the bookmark (in milliseconds since epoch). */
    milliseconds startTimeMs = milliseconds(0);

    /**%apidoc Length of the bookmark (in milliseconds). */
    milliseconds durationMs = milliseconds(0);

    /** \returns End time in milliseconds since epoch. */
    milliseconds endTime() const;

    /**%apidoc[opt]:stringArray List of tags attached to the bookmark. */
    QnCameraBookmarkTags tags;

    /**%apidoc Device id. */
    QnUuid cameraId;

    /**
      * Returns creation time of bookmark in milliseconds since epoch.
      * If bookmark is created by system or by older VMS version returns
      * timestamp that equals to startTimeMs field
      */
    milliseconds creationTime() const;

    bool isCreatedBySystem() const;

    /** @return True if bookmark is null, false otherwise. */
    bool isNull() const;

    /** @return True if bookmark is valid, false otherwise. */
    bool isValid() const;

    QnCameraBookmark();

    bool operator==(const QnCameraBookmark& other) const = default;

    QnCameraBookmark(const QnCameraBookmark&) = default;
    QnCameraBookmark& operator=(const QnCameraBookmark&) = default;

    QnCameraBookmark(QnCameraBookmark&&) noexcept = default;
    QnCameraBookmark& operator=(QnCameraBookmark&&) noexcept = default;

    static QString tagsToString(
        const QnCameraBookmarkTags& tags, const QString& delimiter = QStringLiteral(", "));

    static QnCameraBookmarkList mergeCameraBookmarks(
        nx::vms::common::SystemContext* systemContext,
        const QnMultiServerCameraBookmarkList& source,
        const QnBookmarkSortOrder& sortOrder = QnBookmarkSortOrder::defaultOrder,
        const std::optional<milliseconds>& minVisibleLength = std::nullopt,
        int limit = std::numeric_limits<int>().max());

    static QnUuid systemUserId();

    static const QString kGuidParam;
    static const QString kCreationStartTimeParam;
    static const QString kCreationEndTimeParam;
};
#define QnCameraBookmark_Fields \
    (guid)(creatorId)(creationTimeStampMs)(name)(description)(timeout)\
    (startTimeMs)(durationMs)(tags)(cameraId)

NX_REFLECTION_INSTRUMENT(QnCameraBookmark, QnCameraBookmark_Fields)

void NX_VMS_COMMON_API serialize(
    nx::reflect::json::SerializationContext* ctx, const QnCameraBookmarkTags& value);

nx::reflect::DeserializationResult NX_VMS_COMMON_API deserialize(
    const nx::reflect::json::DeserializationContext& ctx, QnCameraBookmarkTags* data);

/**
 * Bookmarks search request parameters. Used for loading bookmarks for the fixed time period with
 * length exceeding fixed minimal, with name and/or tags containing fixed string.
 */
struct NX_VMS_COMMON_API QnCameraBookmarkSearchFilter
{
    using milliseconds = std::chrono::milliseconds;
    /** Minimum start time for the bookmark. Zero means no limit. */
    milliseconds startTimeMs{};

    /**
     * Time point around which bookmarks are going to be returned. If parameter is specified then
     * request returns nearest (by start time) limit/2 bookmarks before the split point and nearest
     * limit/2 bookmarks after.
     * To derminate nearest bookmarks start time field is taken into consideration. If there are
     * bookmarks with the same start time then guid field is used to determine the order.
     * After bookmarks are gathered they are sorted by the specified column.
     * In case of ascending sorting (whatever the sorting column is) the split point is right after
     * the specified time point. In case of descending order the split point is right before the
     * specified time point.
     * In addition to the sort filed all returned bookmarks are sorted by the guid field.
     */
    std::optional<std::chrono::milliseconds> centralTimePointMs;

    /** Maximum end time for the bookmark. Zero means no limit. */
    milliseconds endTimeMs{};

    /** Text-search filter string. */
    QString text;

    int limit = kNoLimit; // TODO: #sivanov Bookmarks work in merge function only.

    std::optional<milliseconds> minVisibleLengthMs;

    QnBookmarkSortOrder orderBy = QnBookmarkSortOrder::defaultOrder;

    std::optional<QnUuid> id;

    std::chrono::milliseconds creationStartTimeMs{};
    std::chrono::milliseconds creationEndTimeMs{};
    std::set<QnUuid> cameras;

    bool operator==(const QnCameraBookmarkSearchFilter& other) const = default;

    /** Log operator. */
    QString toString() const;

    bool isValid() const;

    bool checkBookmark(const QnCameraBookmark &bookmark) const;

    static const int kNoLimit;
};
#define QnCameraBookmarkSearchFilter_Fields (startTimeMs)(endTimeMs)(text)(limit)(orderBy)\
    (centralTimePointMs)(minVisibleLengthMs)(creationStartTimeMs)(creationEndTimeMs)(id)(cameras)
NX_REFLECTION_INSTRUMENT(QnCameraBookmarkSearchFilter, QnCameraBookmarkSearchFilter_Fields)

struct NX_VMS_COMMON_API QnCameraBookmarkTag
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

    bool operator==(const QnCameraBookmarkTag& other) const = default;

    bool isValid() const
    {
        return !name.isEmpty();
    }

    static QnCameraBookmarkTagList mergeCameraBookmarkTags(
        const QnMultiServerCameraBookmarkTagList &source,
        int limit = std::numeric_limits<int>().max());
};
#define QnCameraBookmarkTag_Fields (name)(count)

NX_VMS_COMMON_API bool operator<(const QnCameraBookmark &first, const QnCameraBookmark &other);
NX_VMS_COMMON_API bool operator<(std::chrono::milliseconds first, const QnCameraBookmark &other);
NX_VMS_COMMON_API bool operator<(const QnCameraBookmark &first, std::chrono::milliseconds other);

NX_VMS_COMMON_API QDebug operator<<(QDebug dbg, const QnCameraBookmark &bookmark);

NX_VMS_COMMON_API QString bookmarkToString(const QnCameraBookmark& bookmark);
NX_VMS_COMMON_API QVariantList bookmarkListToVariantList(const QnCameraBookmarkList& bookmarks);
NX_VMS_COMMON_API QnCameraBookmarkList variantListToBookmarkList(const QVariantList& list);

Q_DECLARE_TYPEINFO(QnCameraBookmark, Q_MOVABLE_TYPE);

QN_FUSION_DECLARE_FUNCTIONS(QnBookmarkSortOrder, (json))
QN_FUSION_DECLARE_FUNCTIONS(QnCameraBookmarkSearchFilter, (json), NX_VMS_COMMON_API)

QN_FUSION_DECLARE_FUNCTIONS(QnCameraBookmark,
    (sql_record)(json)(ubjson)(xml)(csv_record),
    NX_VMS_COMMON_API)
QN_FUSION_DECLARE_FUNCTIONS(QnCameraBookmarkTag,
    (sql_record)(json)(ubjson)(xml)(csv_record),
    NX_VMS_COMMON_API)
