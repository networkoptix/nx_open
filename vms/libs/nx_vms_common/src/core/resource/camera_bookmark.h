// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <limits>
#include <utility>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/reflect/json.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/bookmark_models.h>

#include "camera_bookmark_fwd.h"

namespace nx::vms::common { class SystemContext; }

namespace nx::vms::common {

using namespace std::chrono_literals;

struct NX_VMS_COMMON_API BookmarkSortOrder
{
    nx::vms::api::BookmarkSortField column = nx::vms::api::BookmarkSortField::startTime;
    Qt::SortOrder order = Qt::AscendingOrder;

    explicit BookmarkSortOrder(
        nx::vms::api::BookmarkSortField column = nx::vms::api::BookmarkSortField::startTime,
        Qt::SortOrder order = Qt::AscendingOrder);

    bool operator==(const BookmarkSortOrder& other) const = default;

    static const BookmarkSortOrder defaultOrder;
};
#define BookmarkSortOrder_Fields (column)(order)
NX_REFLECTION_INSTRUMENT(BookmarkSortOrder, BookmarkSortOrder_Fields)

/**
 * Shareable parameters of a Bookmark.
 */
struct NX_VMS_COMMON_API BookmarkShareableParams
{
    bool shareable{false};

    /**%apidoc Time Bookmark expires in milliseconds from epoch. */
    std::chrono::milliseconds expirationTimeMs{0ms};

    /**%apidoc Optional digest of password used to protect access to Bookmark via sharing. */
    std::optional<QString> digest;

    bool operator==(const BookmarkShareableParams& other) const = default;
};
#define BookmarkShareableParams_Fields (shareable)(expirationTimeMs)(digest)
NX_REFLECTION_INSTRUMENT(BookmarkShareableParams, BookmarkShareableParams_Fields)

/**
 * Bookmarked part of the camera archive.
 */
struct NX_VMS_COMMON_API CameraBookmark
{
    /**%apidoc[readonly] Id of the Bookmark. */
    QnUuid guid;

    /**%apidoc[readonly] Identifier of the user who created this Bookmark. */
    QnUuid creatorId;

    /**%apidoc[opt] Time of the Bookmark creation in milliseconds since epoch. */
    std::chrono::milliseconds creationTimeStampMs{0ms};

    /**%apidoc Caption of the Bookmark. */
    QString name;

    /**%apidoc[opt] Details of the Bookmark. */
    QString description;

    /**%apidoc[opt]
     * Time during which a recorded period should be preserved (in milliseconds).
     * %deprecated
     */
    std::chrono::milliseconds timeout{-1ms};

    /**%apidoc Start time of the Bookmark (in milliseconds since epoch). */
    std::chrono::milliseconds startTimeMs{0ms};

    /**%apidoc Length of the Bookmark (in milliseconds). */
    std::chrono::milliseconds durationMs{0ms};

    /** \returns End time in milliseconds since epoch. */
    std::chrono::milliseconds endTime() const;

    /**%apidoc[opt]:stringArray List of tags attached to the Bookmark. */
    QnCameraBookmarkTags tags;

    /**%apidoc Device id. */
    QnUuid cameraId;

    BookmarkShareableParams share;

    /**
     * Returns the creation time of the Bookmark in milliseconds since epoch. If the Bookmark is
     * created by the System or by an older VMS version, returns the timestamp that equals to the
     * startTimeMs field.
     */
    std::chrono::milliseconds creationTime() const;

    bool shareable() const;

    bool createdBySystem() const;

    /** @return Whether the Bookmark is null. */
    bool isNull() const;

    /** @return Whether the Bookmark is valid. */
    bool isValid() const;

    bool operator==(const CameraBookmark& other) const = default;

    static QString tagsToString(
        const QnCameraBookmarkTags& tags, const QString& delimiter = QStringLiteral(", "));

    static QnCameraBookmarkList mergeCameraBookmarks(nx::vms::common::SystemContext* systemContext,
        const QnMultiServerCameraBookmarkList& source,
        const BookmarkSortOrder& sortOrder = BookmarkSortOrder::defaultOrder,
        const std::optional<std::chrono::milliseconds>& minVisibleLength = std::nullopt,
        int limit = std::numeric_limits<int>::max());

    static QnUuid systemUserId();

    static const QString kGuidParam;
    static const QString kCreationStartTimeParam;
    static const QString kCreationEndTimeParam;
};
#define CameraBookmark_Fields \
    (guid)(creatorId)(creationTimeStampMs)(name)(description)(timeout)(startTimeMs)(durationMs) \
    (tags)(cameraId)(share)

NX_REFLECTION_INSTRUMENT(CameraBookmark, CameraBookmark_Fields)

void NX_VMS_COMMON_API serialize(
    nx::reflect::json::SerializationContext* ctx, const QnCameraBookmarkTags& value);

nx::reflect::DeserializationResult NX_VMS_COMMON_API deserialize(
    const nx::reflect::json::DeserializationContext& ctx, QnCameraBookmarkTags* data);

/**
 * Bookmarks search request parameters. Used for loading bookmarks for the fixed time period with
 * length exceeding fixed minimal, with name and/or tags containing fixed string.
 */
struct NX_VMS_COMMON_API CameraBookmarkSearchFilter
{
    using milliseconds = std::chrono::milliseconds;
    /** Minimum start time for the Bookmark. Zero means no limit. */
    milliseconds startTimeMs{};

    /**
     * Time point around which bookmarks are returned. If this parameter is specified then return
     * nearest (by start time) limit/2 Bookmarks before the split point and nearest limit/2
     * Bookmarks after.
     *
     * If there are Bookmarks with the same start time then the `guid` field is used to determine
     * the order. All obtained Bookmarks are sorted by the specified column. If sorting ascending,
     * regardless of sort column, the split point is right after the specified time point. In case
     * of descending order, the split point is right before the specified time point. In addition
     * to the sort column, all returned Bookmarks are sorted by the `guid` field.
     */
    std::optional<std::chrono::milliseconds> centralTimePointMs;

    /** Maximum end time for the Bookmark. Zero means no limit. */
    milliseconds endTimeMs{};

    /** Text-search filter string. */
    QString text;

    int limit = kNoLimit; // TODO: #sivanov Bookmarks work in merge function only.

    std::optional<milliseconds> minVisibleLengthMs;

    BookmarkSortOrder orderBy = BookmarkSortOrder::defaultOrder;

    std::optional<QnUuid> id;

    std::chrono::milliseconds creationStartTimeMs{};
    std::chrono::milliseconds creationEndTimeMs{};
    std::set<QnUuid> cameras;

    bool operator==(const CameraBookmarkSearchFilter& other) const = default;

    /** Log operator. */
    QString toString() const;

    bool isValid() const;

    bool checkBookmark(const CameraBookmark& bookmark) const;

    static const int kNoLimit;
};
#define CameraBookmarkSearchFilter_Fields \
    (startTimeMs)(endTimeMs)(text)(limit)(orderBy)(centralTimePointMs)(minVisibleLengthMs) \
    (creationStartTimeMs)(creationEndTimeMs)(id)(cameras)
NX_REFLECTION_INSTRUMENT(CameraBookmarkSearchFilter, CameraBookmarkSearchFilter_Fields)

struct NX_VMS_COMMON_API CameraBookmarkTag
{
    QString name;
    int count{0};

    bool operator==(const CameraBookmarkTag& other) const = default;

    bool isValid() const { return !name.isEmpty(); }

    static QnCameraBookmarkTagList mergeCameraBookmarkTags(
        const QnMultiServerCameraBookmarkTagList& source,
        int limit = std::numeric_limits<int>::max());
};
#define CameraBookmarkTag_Fields (name)(count)

NX_VMS_COMMON_API bool operator<(const CameraBookmark& first, const CameraBookmark& other);
NX_VMS_COMMON_API bool operator<(std::chrono::milliseconds first, const CameraBookmark& other);
NX_VMS_COMMON_API bool operator<(const CameraBookmark& first, std::chrono::milliseconds other);

NX_VMS_COMMON_API QDebug operator<<(QDebug dbg, const CameraBookmark& bookmark);

NX_VMS_COMMON_API QString bookmarkToString(const CameraBookmark& bookmark);
NX_VMS_COMMON_API QVariantList bookmarkListToVariantList(const QnCameraBookmarkList& bookmarks);
NX_VMS_COMMON_API QnCameraBookmarkList variantListToBookmarkList(const QVariantList& list);

QN_FUSION_DECLARE_FUNCTIONS(BookmarkSortOrder, (json))
QN_FUSION_DECLARE_FUNCTIONS(CameraBookmarkSearchFilter, (json), NX_VMS_COMMON_API)

QN_FUSION_DECLARE_FUNCTIONS(
    CameraBookmark, (sql_record) (json) (ubjson) (xml) (csv_record), NX_VMS_COMMON_API)
QN_FUSION_DECLARE_FUNCTIONS(
    CameraBookmarkTag, (sql_record) (json) (ubjson) (xml) (csv_record), NX_VMS_COMMON_API)

} // namespace nx::vms::common

Q_DECLARE_TYPEINFO(nx::vms::common::CameraBookmark, Q_MOVABLE_TYPE);

// Create aliases to old global namespace names so the rest of the code does not have
// to be refactored to accommodate the new namespaced names.  Remove this code when
// we want to transition the whole system to use the namespaced version.

using QnBookmarkSortOrder = nx::vms::common::BookmarkSortOrder;
using QnCameraBookmark = nx::vms::common::CameraBookmark;
using QnCameraBookmarkSearchFilter = nx::vms::common::CameraBookmarkSearchFilter;
using QnCameraBookmarkTag = nx::vms::common::CameraBookmarkTag;

static const auto bookmarkToString =
    [](const QnCameraBookmark& bookmark)
    {
        return nx::vms::common::bookmarkToString(bookmark);
    };
static const auto bookmarkListToVariantList =
    [](const QnCameraBookmarkList& bookmarks)
    {
        return nx::vms::common::bookmarkListToVariantList(bookmarks);
    };
static const auto variantListToBookmarkList =
    [](const QVariantList& list)
    {
        return nx::vms::common::variantListToBookmarkList(list);
    };
