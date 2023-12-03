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

struct NX_VMS_API BookmarkFilter
{
    BookmarkFilter() {}
    BookmarkFilter(QnUuid id, nx::vms::api::json::ValueOrArray<QString> deviceId = {}):
        id(std::move(id)), deviceId(std::move(deviceId))
    {
    }

    std::optional<QnUuid> id;

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
     * In addition to the sort field all returned bookmarks are sorted by the guid field.
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
#define BookmarkFilter_Fields \
    (id)(startTimeMs)(endTimeMs)(centralTimePointMs)(text)(limit)(order)(column) \
    (minVisibleLengthMs)(deviceId)(creationStartTimeMs)(creationEndTimeMs)
QN_FUSION_DECLARE_FUNCTIONS(BookmarkFilter, (json), NX_VMS_API)

struct NX_VMS_API Bookmark
{
    /**%apidoc[readonly] Id of the Bookmark. */
    QnUuid id;

    /**%apidoc Device id. */
    QString deviceId;

    /**%apidoc[immutable] Server id where Bookmark is stored. */
    QnUuid serverId;

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

    QnUuid getId() const { return id; }
    void setId(QnUuid id_) { id = std::move(id_); }

    bool operator==(const Bookmark& other) const = default;
};
#define Bookmark_Fields \
    (id) \
    (deviceId) \
    (serverId) \
    (name) \
    (description) \
    (startTimeMs) \
    (durationMs) \
    (tags) \
    (creatorUserId) \
    (creationTimeMs)
QN_FUSION_DECLARE_FUNCTIONS(Bookmark, (json), NX_VMS_API)

struct NX_VMS_API BookmarkWithRule: Bookmark
{
     std::optional<QnUuid> eventRuleId;
};
#define BookmarkWithRule_Fields Bookmark_Fields (eventRuleId)
QN_FUSION_DECLARE_FUNCTIONS(BookmarkWithRule, (json), NX_VMS_API)

struct NX_VMS_API BookmarkTagFilter
{
    // TODO: Ad support for id and deviceId.
    std::optional<int> limit;
};
#define BookmarkTagFilter_Fields (limit)
QN_FUSION_DECLARE_FUNCTIONS(BookmarkTagFilter, (json), NX_VMS_API)

using BookmarkTagCounts = Map<QString /*tag*/, int /*count*/>;

} // namespace nx::vms::api
