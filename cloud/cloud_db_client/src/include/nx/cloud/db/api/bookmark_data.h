// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <string>

#include <nx/reflect/instrument.h>
#include <nx/reflect/json/deserializer.h>
#include <nx/utils/url_query.h>

namespace nx::cloud::db::api {

struct Bookmark
{
    /**
     * Time of the bookmark creation in milliseconds since epoch.
     */
    std::chrono::milliseconds creationTimeMs{-1};

    /**
     * Identifier of the user who created this bookmark.
     */
    std::optional<std::string> creatorUserId;

    /**
     * Details of the bookmark.
     */
    std::optional<std::string> description;

    /**
     * Device id. (Required field)
     */
    std::string deviceId;

    /**
     * Length of the bookmark in milliseconds.
     */
    std::chrono::milliseconds durationMs{-1};

    /**
     * Id of the bookmark.
     */
    std::optional<std::string> id;

    /**
     * Caption of the bookmark.
     */
    std::optional<std::string> name;

    /**
     * Organization ID. (Required field)
     */
    std::string organizationId;

    /**
     * Site ID. (Required field)
     */
    std::string siteId;

    /**
     * Start time of the bookmark in milliseconds since epoch.
     */
    std::chrono::milliseconds startTimeMs{-1};

    /**
     * List of tags attached to the bookmark.
     */
    std::optional<std::vector<std::string>> tags;

    bool operator==(const Bookmark& other) const = default;
};

#define Bookmark_Fields \
    (creationTimeMs) \
    (creatorUserId) \
    (description) \
    (deviceId) \
    (durationMs) \
    (id) \
    (name) \
    (organizationId) \
    (siteId) \
    (startTimeMs) \
    (tags)

NX_REFLECTION_INSTRUMENT(Bookmark, Bookmark_Fields)
NX_REFLECTION_TAG_TYPE(Bookmark, jsonSerializeChronoDurationAsNumber)

struct BookmarkTag
{
    int64_t count = -1;
    std::string text;
};

#define BookmarkTag_Fields \
    (count) \
    (text)

NX_REFLECTION_INSTRUMENT(BookmarkTag, BookmarkTag_Fields)

struct Version
{
    std::string revision;
    std::string version;
};

#define Version_Fields \
    (revision) \
    (version)

NX_REFLECTION_INSTRUMENT(Version, Version_Fields)

struct GetBookmarkFilter
{
    std::optional<std::string> organizationId;
    std::optional<std::vector<std::string>> deviceId;
    std::optional<std::vector<std::string>> siteId;
    std::optional<std::string> id;
    std::optional<std::chrono::milliseconds> startTimeMs;
    std::optional<std::chrono::milliseconds> endTimeMs;
    std::optional<std::string> text;
    std::optional<int64_t> firstItemNum;
    std::optional<int64_t> limit;
    std::optional<std::string> order;
    std::optional<std::string> column;
    std::optional<std::chrono::milliseconds> minVisibleLengthMs;
    std::optional<std::chrono::milliseconds> creationStartTimeMs;
    std::optional<std::chrono::milliseconds> creationEndTimeMs;

    nx::UrlQuery toUrlQuery() const;
};

#define GetBookmarksParams_Fields \
    (organizationId) \
    (deviceId) \
    (siteId) \
    (id) \
    (startTimeMs) \
    (endTimeMs) \
    (text) \
    (firstItemNum) \
    (limit) \
    (order) \
    (column) \
    (minVisibleLengthMs) \
    (creationStartTimeMs) \
    (creationEndTimeMs)

NX_REFLECTION_INSTRUMENT(GetBookmarkFilter, GetBookmarksParams_Fields)
NX_REFLECTION_TAG_TYPE(GetBookmarkFilter, jsonSerializeChronoDurationAsNumber)

struct DeleteBookmarkFilter
{
    std::string organizationId;
    std::vector<std::string> siteId;

    std::optional<std::vector<std::string>> deviceId;
    std::optional<std::string> id;
    std::optional<std::chrono::milliseconds> startTimeMs;
    std::optional<std::chrono::milliseconds> endTimeMs;
    std::optional<std::string> text;

    nx::UrlQuery toUrlQuery() const;
};

#define DeleteBookmarksParams_Fields \
    (organizationId) \
    (deviceId) \
    (siteId) \
    (id) \
    (startTimeMs) \
    (endTimeMs) \
    (text)

NX_REFLECTION_INSTRUMENT(DeleteBookmarkFilter, DeleteBookmarksParams_Fields)
NX_REFLECTION_TAG_TYPE(DeleteBookmarkFilter, jsonSerializeChronoDurationAsNumber)

struct GetBookmarkTagFilter
{
    std::optional<std::string> organizationId;
    std::optional<std::vector<std::string>> siteId;
    std::optional<int64_t> limit;

    nx::UrlQuery toUrlQuery() const;
};

#define GetBookmarkTagsParams_Fields \
    (organizationId) \
    (siteId) \
    (limit)

NX_REFLECTION_INSTRUMENT(GetBookmarkTagFilter, GetBookmarkTagsParams_Fields)

} // namespace nx::cloud::db::api
