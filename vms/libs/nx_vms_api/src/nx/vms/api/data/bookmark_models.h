// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <map>
#include <optional>
#include <set>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/utils/serialization/qt_core_types.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/json/value_or_array.h>

#include "map.h"

namespace nx::vms::api {

// All sorts are preformed manually, not in the SQL database request.
NX_REFLECTION_ENUM_CLASS(BookmarkSortField,
    name,
    startTime,
    duration,
    creationTime,
    creator,
    tags,
    description,
    cameraName
)

struct NX_VMS_API BookmarkFilterBase
{
    BookmarkFilterBase(nx::vms::api::json::ValueOrArray<QString> deviceId = {}):
        deviceId(std::move(deviceId))
    {
    }

    /**%apidoc[opt] Minimum start time for the bookmark. */
    std::chrono::milliseconds startTimeMs{};

    /**%apidoc[opt] Maximum end time for the bookmark. */
    std::chrono::milliseconds endTimeMs{};

    /**%apidoc[opt]
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

    /**%apidoc[opt] Text-search filter string. */
    QString text;

    /**%apidoc Returned bookmark count limit. */
    std::optional<int> limit;

    /**%apidoc[opt]:enum
     * Result Bookmarks order.
     * %value asc
     * %value desc
     */
    Qt::SortOrder order = Qt::AscendingOrder;

    /**%apidoc[opt] Bookmark field used for ordering. */
    BookmarkSortField column = BookmarkSortField::startTime;

    /**%apidoc[opt] Do not return bookmarks with duration less than this value. */
    std::optional<std::chrono::milliseconds> minVisibleLengthMs;

    /**%apidoc:stringArray Device ids to get Bookmarks on. */
    nx::vms::api::json::ValueOrArray<QString> deviceId;

    /**%apidoc[opt] Minimum creation time of the bookmark (in milliseconds since epoch, or as a string). */
    std::chrono::milliseconds creationStartTimeMs{};

    /**%apidoc[opt] Maximum creation time of the bookmark (in milliseconds since epoch, or as a string) */
    std::chrono::milliseconds creationEndTimeMs{};
};
#define BookmarkFilterBase_Fields \
    (startTimeMs)(endTimeMs)(centralTimePointMs)(text)(limit)(order)(column) \
    (minVisibleLengthMs)(deviceId)(creationStartTimeMs)(creationEndTimeMs)

struct BookmarkIdV1
{
    /**%apidoc[opt] Bookmark id. */
    QnUuid id;

    BookmarkIdV1(QnUuid id = {}): id(std::move(id)) {}
    const QnUuid& bookmarkId() const { return id; }
    QnUuid serverId() const { return QnUuid(); }

    const QnUuid& getId() const { return id; }
    void setId(QnUuid id_) { id = std::move(id_); }
    QString toString() const { return id.toSimpleString(); }
};
#define BookmarkIdV1_Fields (id)
QN_FUSION_DECLARE_FUNCTIONS(BookmarkIdV1, (json), NX_VMS_API)

struct NX_VMS_API BookmarkIdV3
{
    /**%apidoc[opt] Combined Bookmark and Server ids `{bookmarkId}_{serverId}`. */
    QString id;

    BookmarkIdV3(QString id = {}): id(std::move(id)) {}
    QnUuid bookmarkId() const;
    QnUuid serverId() const;
    void setIds(const QnUuid& bookmarkId, const QnUuid& serverId);

    QString getId() const { return id; }
    void setId(const QnUuid& id_) { id = id_.toSimpleString(); }
    const QString& toString() const { return id; }
};
#define BookmarkIdV3_Fields (id)
QN_FUSION_DECLARE_FUNCTIONS(BookmarkIdV3, (json), NX_VMS_API)

struct NX_VMS_API BookmarkFilterV1: BookmarkFilterBase, BookmarkIdV1
{
    using BookmarkIdV1::BookmarkIdV1;
};
#define BookmarkFilterV1_Fields BookmarkFilterBase_Fields(id)
QN_FUSION_DECLARE_FUNCTIONS(BookmarkFilterV1, (json), NX_VMS_API)

struct NX_VMS_API BookmarkFilterV3: BookmarkFilterBase, BookmarkIdV3
{
    using BookmarkIdV3::BookmarkIdV3;
};
#define BookmarkFilterV3_Fields BookmarkFilterBase_Fields(id)
QN_FUSION_DECLARE_FUNCTIONS(BookmarkFilterV3, (json), NX_VMS_API)

struct NX_VMS_API BookmarkBase
{
    /**%apidoc Device id. */
    QnUuid deviceId;

    /**%apidoc Caption of the Bookmark.
     * %example Bookmark
     */
    QString name;

    /**%apidoc[opt] Details of the Bookmark. */
    QString description;

    /**%apidoc[opt] Start time of the Bookmark (in milliseconds since epoch). */
    std::chrono::milliseconds startTimeMs{0};

    /**%apidoc Length of the Bookmark (in milliseconds).
     * %example 1000
     */
    std::chrono::milliseconds durationMs{0};

    /**%apidoc[opt]:stringArray List of tags attached to the Bookmark. */
    std::set<QString> tags;

    /**%apidoc[readonly] Id of the User created this bookmark. */
    std::optional<QnUuid> creatorUserId;

    /**%apidoc[readonly]
     * Time of the Bookmark creation in milliseconds since epoch. Equals to startTimeMs field if
     * the Bookmark is created by the System.
     */
    std::optional<std::chrono::milliseconds> creationTimeMs{0};

};
#define BookmarkBase_Fields \
    (deviceId) \
    (name) \
    (description) \
    (startTimeMs) \
    (durationMs) \
    (tags) \
    (creatorUserId) \
    (creationTimeMs)

/**%apidoc
 * %param[readonly] id
 */
struct NX_VMS_API BookmarkV1: BookmarkBase, BookmarkIdV1
{
    /**%apidoc[immutable] Server id where Bookmark is stored. */
    QnUuid serverId;

    using BookmarkIdV1::BookmarkIdV1;
    void setIds(QnUuid bookmarkId, QnUuid serverId)
    {
        id = std::move(bookmarkId);
        this->serverId = std::move(serverId);
    }
};
#define BookmarkV1_Fields BookmarkBase_Fields BookmarkIdV1_Fields (serverId)
QN_FUSION_DECLARE_FUNCTIONS(BookmarkV1, (json), NX_VMS_API)

struct NX_VMS_API BookmarkWithRuleV1: BookmarkV1
{
    std::optional<QnUuid> eventRuleId;

    using Bookmark = BookmarkV1;
    using Filter = BookmarkFilterV1;
    using Id = BookmarkIdV1;
};
#define BookmarkWithRuleV1_Fields BookmarkV1_Fields (eventRuleId)
QN_FUSION_DECLARE_FUNCTIONS(BookmarkWithRuleV1, (json), NX_VMS_API)

/**%apidoc
 * %param[readonly] id
 */
struct NX_VMS_API BookmarkV3: BookmarkBase, BookmarkIdV3
{
     using BookmarkIdV3::BookmarkIdV3;
};
#define BookmarkV3_Fields BookmarkBase_Fields BookmarkIdV3_Fields
QN_FUSION_DECLARE_FUNCTIONS(BookmarkV3, (json), NX_VMS_API)

using Bookmark = BookmarkV3;

struct NX_VMS_API BookmarkWithRuleV3: BookmarkV3
{
    std::optional<QnUuid> eventRuleId;

    using Bookmark = BookmarkV3;
    using Filter = BookmarkFilterV3;
    using Id = BookmarkIdV3;
};
#define BookmarkWithRuleV3_Fields BookmarkV3_Fields (eventRuleId)
QN_FUSION_DECLARE_FUNCTIONS(BookmarkWithRuleV3, (json), NX_VMS_API)

using BookmarkWithRule = BookmarkWithRuleV3;

struct NX_VMS_API BookmarkTagFilter
{
    // TODO: Ad support for id and deviceId.
    std::optional<int> limit;
};
#define BookmarkTagFilter_Fields (limit)
QN_FUSION_DECLARE_FUNCTIONS(BookmarkTagFilter, (json), NX_VMS_API)

using BookmarkTagCounts = Map<QString /*tag*/, int /*count*/>;

} // namespace nx::vms::api
