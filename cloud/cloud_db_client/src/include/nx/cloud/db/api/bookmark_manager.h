// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/move_only_func.h>

#include "result_code.h"
#include "bookmark_data.h"

namespace nx::cloud::db::api {

class BookmarkManager
{
public:
    using ResultCodeHandler = nx::MoveOnlyFunc<void(ResultCode)>;
    using GetBookmarksHandler = nx::MoveOnlyFunc<void(ResultCode, std::vector<Bookmark>)>;
    using GetBookmarkTagsHandler = nx::MoveOnlyFunc<void(ResultCode, std::vector<BookmarkTag>)>;
    using PatchBookmarkHandler = nx::MoveOnlyFunc<void(ResultCode, Bookmark)>;
    using GetVersionHandler = nx::MoveOnlyFunc<void(ResultCode, Version)>;

    virtual ~BookmarkManager() = default;

    virtual void saveBookmarks(std::vector<Bookmark> bookmarks, ResultCodeHandler handler) = 0;
    virtual ResultCode saveBookmarksSync(std::vector<Bookmark> bookmarks) = 0;

    virtual void patchBookmark(Bookmark bookmark, ResultCodeHandler handler) = 0;
    virtual ResultCode patchBookmarkSync(Bookmark bookmark) = 0;

    virtual void getBookmarks(GetBookmarkFilter filter, GetBookmarksHandler handler) = 0;
    virtual std::pair<ResultCode, std::vector<Bookmark>> getBookmarksSync(
        GetBookmarkFilter filter) = 0;

    virtual void getBookmarkTags(GetBookmarkTagFilter filter, GetBookmarkTagsHandler handler) = 0;
    virtual std::pair<ResultCode, std::vector<BookmarkTag>> getBookmarkTagsSync(
        GetBookmarkTagFilter filter) = 0;

    virtual void deleteBookmarks(DeleteBookmarkFilter filter, ResultCodeHandler handler) = 0;
    virtual ResultCode deleteBookmarksSync(DeleteBookmarkFilter filter) = 0;
    virtual void getVersion(GetVersionHandler handler) = 0;
    virtual std::pair<ResultCode, Version> getVersionSync() = 0;
};

} // namespace nx::cloud::db::api
