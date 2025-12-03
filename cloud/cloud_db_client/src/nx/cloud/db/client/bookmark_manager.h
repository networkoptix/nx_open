// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/cloud/db/api/bookmark_manager.h>

#include "async_http_requests_executor.h"


namespace nx::cloud::db::client {

class BookmarkManager: public api::BookmarkManager
{
public:
    BookmarkManager(ApiRequestsExecutor* requestsExecutor);

    /** Create a new bookmark. */
    virtual void saveBookmarks(std::vector<api::Bookmark> bookmarks, ResultCodeHandler handler) override;

    /** Create a new bookmark. (Sync)*/
    virtual api::ResultCode saveBookmarksSync(std::vector<api::Bookmark> bookmarks) override;

    /** Update an existing bookmark. */
    virtual void patchBookmark(api::Bookmark bookmark, ResultCodeHandler handler) override;

    /** Update an existing bookmark. (Sync)*/
    virtual api::ResultCode patchBookmarkSync(api::Bookmark bookmark) override;

    /** Get bookmarks matching the filter. */
    virtual void getBookmarks(api::GetBookmarkFilter filter, GetBookmarksHandler handler) override;

    /** Get bookmarks matching the filter.  (Sync) */
    virtual std::pair<api::ResultCode, std::vector<api::Bookmark>> getBookmarksSync(
        api::GetBookmarkFilter filter) override;

    /** Get tags matching the filter. */
    virtual void getBookmarkTags(api::GetBookmarkTagFilter filter, GetBookmarkTagsHandler handler) override;

    /** Get tags matching the filter.  (Sync) */
    virtual std::pair<api::ResultCode, std::vector<api::BookmarkTag>> getBookmarkTagsSync(
        api::GetBookmarkTagFilter filter) override;

    /** Delete bookmarks matching the filter. */
    virtual void deleteBookmarks(api::DeleteBookmarkFilter filter, ResultCodeHandler handler) override;

    /** Delete bookmarks matching the filter. (Sync) */
    virtual api::ResultCode deleteBookmarksSync(api::DeleteBookmarkFilter filter) override;

    /** Get the service version information. */
    virtual void getVersion(GetVersionHandler handler) override;

    /** Get the service version information. (Sync) */
    virtual std::pair<api::ResultCode, api::Version> getVersionSync() override;

private:
    ApiRequestsExecutor* m_requestsExecutor;
};

} // namespace nx::cloud::db::client
