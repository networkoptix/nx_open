// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bookmark_manager.h"

#include <nx/network/http/rest/http_rest_client.h>
#include <nx/utils/sync_call.h>

namespace nx::cloud::db::client {

namespace {

static const std::string kBasePath = "/bookmarks/v4";

} // namespace

BookmarkManager::BookmarkManager(ApiRequestsExecutor* requestsExecutor):
    m_requestsExecutor(requestsExecutor)
{
}

using namespace nx::network::http;

void BookmarkManager::saveBookmarks(
    std::vector<api::Bookmark> bookmarks, ResultCodeHandler handler)
{
    m_requestsExecutor->makeAsyncCall<void>(
        Method::post,
        kBasePath + "/internal/bookmarks",
        nx::UrlQuery(),
        std::move(bookmarks),
        std::move(handler));
}

api::ResultCode BookmarkManager::saveBookmarksSync(std::vector<api::Bookmark> bookmarks)
{
    api::ResultCode resCode = api::ResultCode::ok;

    std::tie(resCode) =
        makeSyncCall<api::ResultCode>(
            std::bind(
                &BookmarkManager::saveBookmarks,
                this,
                std::move(bookmarks),
                std::placeholders::_1));

    return resCode;
}

void BookmarkManager::patchBookmark(api::Bookmark bookmark, ResultCodeHandler handler)
{
    m_requestsExecutor->makeAsyncCall<void>(
        Method::patch,
        kBasePath + "/internal/bookmarks/" + *bookmark.id,
        nx::UrlQuery(),
        std::move(bookmark),
        std::move(handler));
}
api::ResultCode BookmarkManager::patchBookmarkSync(api::Bookmark bookmark)
{
    api::ResultCode resCode = api::ResultCode::ok;

    std::tie(resCode) =
        makeSyncCall<api::ResultCode>(
            std::bind(
                &BookmarkManager::patchBookmark,
                this,
                std::move(bookmark),
                std::placeholders::_1));

    return resCode;
}

void BookmarkManager::getBookmarks(api::GetBookmarkFilter filter, GetBookmarksHandler handler)
{
    m_requestsExecutor->makeAsyncCall<std::vector<api::Bookmark>>(
        Method::get,
        kBasePath + "/bookmarks",
        filter.toUrlQuery(),
        std::move(handler));
}

std::pair<api::ResultCode, std::vector<api::Bookmark>> BookmarkManager::getBookmarksSync(
    api::GetBookmarkFilter filter)
{
    api::ResultCode resCode = api::ResultCode::ok;
    std::vector<api::Bookmark> bookmarks;

    std::tie(resCode, bookmarks) =
        makeSyncCall<api::ResultCode, std::vector<api::Bookmark>>(
            std::bind(
                &BookmarkManager::getBookmarks,
                this,
                std::move(filter),
                std::placeholders::_1));

    return std::make_pair(resCode, std::move(bookmarks));
}

void BookmarkManager::getBookmarkTags(api::GetBookmarkTagFilter filter, GetBookmarkTagsHandler handler)
{
    m_requestsExecutor->makeAsyncCall<std::vector<api::BookmarkTag>>(
        Method::get,
        kBasePath + "/tags",
        filter.toUrlQuery(),
        std::move(handler));
}

std::pair<api::ResultCode, std::vector<api::BookmarkTag>> BookmarkManager::getBookmarkTagsSync(
    api::GetBookmarkTagFilter filter)
{
    api::ResultCode resCode = api::ResultCode::ok;
    std::vector<api::BookmarkTag> tags;

    std::tie(resCode, tags) =
        makeSyncCall<api::ResultCode, std::vector<api::BookmarkTag>>(
            std::bind(
                &BookmarkManager::getBookmarkTags,
                this,
                std::move(filter),
                std::placeholders::_1));

    return std::make_pair(resCode, std::move(tags));
}

void BookmarkManager::deleteBookmarks(api::DeleteBookmarkFilter filter, ResultCodeHandler handler)
{
    m_requestsExecutor->makeAsyncCall<void>(
        Method::delete_,
        kBasePath + "/internal/bookmarks",
        filter.toUrlQuery(),
        std::move(handler));
}

api::ResultCode BookmarkManager::deleteBookmarksSync(api::DeleteBookmarkFilter filter)
{
    api::ResultCode resCode = api::ResultCode::ok;

    std::tie(resCode) =
        makeSyncCall<api::ResultCode>(
            std::bind(
                &BookmarkManager::deleteBookmarks,
                this,
                std::move(filter),
                std::placeholders::_1));

    return resCode;
}

void BookmarkManager::getVersion(GetVersionHandler handler)
{
    m_requestsExecutor->makeAsyncCall<api::Version>(
        Method::get,
        kBasePath + "/maintenance/version",
        nx::UrlQuery(),
        std::move(handler));
}

std::pair<api::ResultCode, api::Version> BookmarkManager::getVersionSync()
{
    api::ResultCode resCode = api::ResultCode::ok;
    api::Version version;

    std::tie(resCode, version) =
        makeSyncCall<api::ResultCode, api::Version>(
            std::bind(
                &BookmarkManager::getVersion,
                this,
                std::placeholders::_1));

    return std::make_pair(resCode, std::move(version));
}

} // namespace nx::cloud::db::client
