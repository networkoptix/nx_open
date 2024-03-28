// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <optional>
#include <set>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
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

/**%apidoc
 * Flags describing filters for shareable bookmarks
 */
NX_REFLECTION_ENUM_CLASS(BookmarkShareFilter,
    none = 0,
    shareable = 1 << 0,
    nonShareable = 1 << 1,
    accessible = 1 << 2,
    hasExpiration = 1 << 3,
    expired = 1 << 4,
    passwordProtected = 1 << 5
)

Q_DECLARE_FLAGS(BookmarkShareFilters, BookmarkShareFilter)
Q_DECLARE_OPERATORS_FOR_FLAGS(BookmarkShareFilters)

struct NX_VMS_API BookmarkFilterBase
{
    BookmarkFilterBase(nx::vms::api::json::ValueOrArray<QString> deviceId = {}):
        deviceId(std::move(deviceId))
    {
    }

    /**%apidoc[opt] Minimum start time for the Bookmark. */
    std::chrono::milliseconds startTimeMs{};

    /**%apidoc[opt] Maximum end time for the Bookmark. */
    std::chrono::milliseconds endTimeMs{};

    /**%apidoc[opt]
     * Time point around which bookmarks are going to be returned. If this parameter is specified
     * then the request returns the nearest limit/2 bookmarks before the split point and the
     * nearest limit/2 bookmarks after.
     * The start time of the bookmarks is used as the time to compare this field against. If there
     * are bookmarks with the same start time then the deviceId field is used to determine the
     * order.
     * After bookmarks are gathered they are sorted by the specified column. In case of ascending
     * order, the split point is right after the specified time point. In case of descending order
     * the split point is right before the specified time point. In addition to the sort field, all
     * returned bookmarks are sorted by deviceId.
     */
    std::optional<std::chrono::milliseconds> centralTimePointMs;

    /**%apidoc[opt]
     * Text-search filter string. Bookmarks can be found by their description or tag. This filter
     * can contain several words, any of them can match a bookmark. Each of these words can match
     * a bookmark if the caption or tag contains the Text-search word in the middle of its value.
     * For example: a bookmark has tag1="12345", the search string "234" will match the bookmark.
    */
    QString text;

    /**%apidoc Returned bookmark count limit. */
    std::optional<int> limit;

    /**%apidoc[opt]:enum
     * Result Bookmarks order.
     * %value asc
     * %value desc
     */
    Qt::SortOrder order{Qt::AscendingOrder};

    /**%apidoc[opt] Bookmark field used for ordering. */
    BookmarkSortField column{BookmarkSortField::startTime};

    /**%apidoc[opt] Minimum duration of returned Bookmarks. */
    std::optional<std::chrono::milliseconds> minVisibleLengthMs;

    /**%apidoc:stringArray Device ids to get Bookmarks on. */
    nx::vms::api::json::ValueOrArray<QString> deviceId;

    /**%apidoc[opt]
     * Minimum creation time of the Bookmark (in milliseconds since epoch, or as a string).
     */
    std::chrono::milliseconds creationStartTimeMs{};

    /**%apidoc[opt]
     * Maximum creation time of the Bookmark (in milliseconds since epoch, or as a string).
     */
    std::chrono::milliseconds creationEndTimeMs{};
};
#define BookmarkFilterBase_Fields \
    (startTimeMs)(endTimeMs)(centralTimePointMs)(text)(limit)(order)(column) \
    (minVisibleLengthMs)(deviceId)(creationStartTimeMs)(creationEndTimeMs)

struct NX_VMS_API BookmarkSharingSettings
{
    /**%apidoc[opt] Time the shareable Bookmark expires in milliseconds since epoch. */
    std::chrono::milliseconds expirationTimeMs{0};

    /**%apidoc[opt] Password required to access the Bookmark. */
    QString password;
};
#define BookmarkSharingSettings_Fields (expirationTimeMs)(password)
QN_FUSION_DECLARE_FUNCTIONS(BookmarkSharingSettings, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(BookmarkSharingSettings, BookmarkSharingSettings_Fields)

struct NX_VMS_API BookmarkIdV1
{
    /**%apidoc[opt] Bookmark id. */
    nx::Uuid id;

    BookmarkIdV1(nx::Uuid id = {}): id(id) {}
    const nx::Uuid& bookmarkId() const { return id; }
    nx::Uuid serverId() const { return {}; }

    const nx::Uuid& getId() const { return id; }
    void setId(nx::Uuid id_) { id = id_; }
    QString toString() const { return id.toSimpleString(); }
};
#define BookmarkIdV1_Fields (id)
QN_FUSION_DECLARE_FUNCTIONS(BookmarkIdV1, (json), NX_VMS_API)

struct NX_VMS_API BookmarkIdV3
{
    /**%apidoc[opt] Combined Bookmark and Server ids `{bookmarkId}_{serverId}`. */
    QString id;

    static nx::Uuid bookmarkIdFromCombined(const QString& id);
    static nx::Uuid serverIdFromCombined(const QString& id);

    BookmarkIdV3(QString id = {}): id(std::move(id)) {}
    nx::Uuid bookmarkId() const;
    nx::Uuid serverId() const;
    void setIds(const nx::Uuid& bookmarkId, const nx::Uuid& serverId);

    QString getId() const { return id; }
    void setId(const nx::Uuid& id_) { id = id_.toSimpleString(); }
    const QString& toString() const { return id; }
};
#define BookmarkIdV3_Fields (id)
QN_FUSION_DECLARE_FUNCTIONS(BookmarkIdV3, (json), NX_VMS_API)

struct NX_VMS_API BookmarkFilterV1: BookmarkFilterBase, BookmarkIdV1
{
    using BookmarkIdV1::BookmarkIdV1;
};
#define BookmarkFilterV1_Fields BookmarkFilterBase_Fields BookmarkIdV1_Fields
QN_FUSION_DECLARE_FUNCTIONS(BookmarkFilterV1, (json), NX_VMS_API)

struct NX_VMS_API BookmarkFilterV3: BookmarkFilterBase, BookmarkIdV3
{
    using BookmarkIdV3::BookmarkIdV3;

    /**%apidoc[opt] */
    BookmarkShareFilters shareFilter = BookmarkShareFilter::none;
};
#define BookmarkFilterV3_Fields BookmarkFilterBase_Fields BookmarkIdV3_Fields(shareFilter)
QN_FUSION_DECLARE_FUNCTIONS(BookmarkFilterV3, (json), NX_VMS_API)

struct NX_VMS_API BookmarkBase
{
    /**%apidoc Device id. */
    nx::Uuid deviceId;

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
    std::optional<nx::Uuid> creatorUserId;

    /**%apidoc[readonly]
     * Time of the Bookmark creation in milliseconds since epoch. Equals to startTimeMs field if
     * the Bookmark is created by the Site.
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
QN_FUSION_DECLARE_FUNCTIONS(BookmarkBase, (json), NX_VMS_API)

/**%apidoc
 * %param[readonly] id
 */
struct NX_VMS_API BookmarkV1: BookmarkBase, BookmarkIdV1
{
    /**%apidoc[immutable] Server id where Bookmark is stored. */
    nx::Uuid serverId;

    using BookmarkIdV1::BookmarkIdV1;
    void setIds(nx::Uuid bookmarkId, nx::Uuid serverId_)
    {
        id = bookmarkId;
        serverId = serverId_;
    }
};
#define BookmarkV1_Fields BookmarkBase_Fields BookmarkIdV1_Fields (serverId)
QN_FUSION_DECLARE_FUNCTIONS(BookmarkV1, (json), NX_VMS_API)

struct NX_VMS_API BookmarkWithRuleV1: BookmarkV1
{
    std::optional<nx::Uuid> eventRuleId;

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
    std::optional<std::variant<std::nullptr_t, BookmarkSharingSettings>> share = std::nullopt;
};
#define BookmarkV3_Fields BookmarkBase_Fields BookmarkIdV3_Fields (share)
QN_FUSION_DECLARE_FUNCTIONS(BookmarkV3, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(BookmarkV3, BookmarkV3_Fields)

using Bookmark = BookmarkV3;

struct NX_VMS_API BookmarkWithRuleV3: BookmarkV3
{
    std::optional<nx::Uuid> eventRuleId;

    using Bookmark = BookmarkV3;
    using Filter = BookmarkFilterV3;
    using Id = BookmarkIdV3;
};
#define BookmarkWithRuleV3_Fields BookmarkV3_Fields (eventRuleId)
QN_FUSION_DECLARE_FUNCTIONS(BookmarkWithRuleV3, (json), NX_VMS_API)

struct NX_VMS_API BookmarkTagFilter
{
    // TODO: Ad support for id and deviceId.
    std::optional<int> limit;
};
#define BookmarkTagFilter_Fields (limit)
QN_FUSION_DECLARE_FUNCTIONS(BookmarkTagFilter, (json), NX_VMS_API)

struct NX_VMS_API BookmarkProtection
{
    static QString getDigest(const QString& bookmarkId, const QString& password);
    static QString getProtection(const QString& digest, std::chrono::milliseconds syncTime);
    static std::chrono::milliseconds getSyncTime(const QString& protection);

    /**%apidoc[opt]
     * Password protection used to authenticate the request if the Bookmark is password protected.
     * Should be calculated as
     * `synchronizedTimeMs + ":" + sha256hex(sha256hex(bookmarkId + password) + synchronizedTimeMs))`
     * where `synchronizedTimeMs` should be obtained from `/rest/v{4-}/site/info`. Example:
     * ```
     * bookmarkId = '997d0166-0479-473f-8578-9b1c5aee14c6_00000000-8aeb-7d56-2bc7-67afae00335c'
     * password = 'password123'
     * synchronizedTimeMs = 1707754215123
     * sha256hex(997d0166-0479-473f-8578-9b1c5aee14c6_00000000-8aeb-7d56-2bc7-67afae00335cpassword123)
     * -- adad72a3a64631cfdbe5726b3c7a314df664f34905fae81266d302a91135b8c7
     * sha256hex(adad72a3a64631cfdbe5726b3c7a314df664f34905fae81266d302a91135b8c71707754215123)
     * -- 0e91c8bb106a4fcf1a407d896a21c6a96a7ed40c6161403af99985c3f1d405f7
     * ?passwordProtection=1707754215123:0e91c8bb106a4fcf1a407d896a21c6a96a7ed40c6161403af99985c3f1d405f7
     * ```
     */
    QString passwordProtection;
};
#define BookmarkProtection_Fields (passwordProtection)

struct NX_VMS_API BookmarkDescriptionRequest: BookmarkIdV3, BookmarkProtection
{
    using BookmarkIdV3::BookmarkIdV3;
};
#define BookmarkDescriptionRequest_Fields BookmarkIdV3_Fields BookmarkProtection_Fields
NX_REFLECTION_INSTRUMENT(BookmarkDescriptionRequest, BookmarkDescriptionRequest_Fields);
QN_FUSION_DECLARE_FUNCTIONS(BookmarkDescriptionRequest, (json), NX_VMS_API)

using BookmarkTagCounts = Map<QString /*tag*/, int /*count*/>;

} // namespace nx::vms::api
