// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <utility>

#include <core/resource/camera_bookmark.h>
#include <nx/utils/member_detector.h>
#include <nx/utils/qt_helpers.h>
#include <nx/utils/std_helpers.h>
#include <nx/vms/api/data/bookmark_models.h>

namespace nx::vms::common {

NX_UTILS_DECLARE_FIELD_DETECTOR_SIMPLE(hasShare, share);
NX_UTILS_DECLARE_FIELD_DETECTOR_SIMPLE(hasShareFilter, shareFilter);

template<typename BookmarkWithRule>
common::BookmarkShareableParams shareableParams(
    const BookmarkWithRule& data, bool digestPassword = true)
{
    if constexpr (hasShare<BookmarkWithRule>::value)
    {
        if (data.share && std::holds_alternative<api::BookmarkSharingSettings>(data.share.value()))
        {
            auto& sharingSettings = std::get<api::BookmarkSharingSettings>(data.share.value());
            std::optional<QString> digest;
            if (!sharingSettings.password.isEmpty())
            {
                if (digestPassword)
                {
                    digest = api::BookmarkProtection::getDigest(
                        data.bookmarkId(), data.serverId(), sharingSettings.password);
                }
                else
                {
                    digest = sharingSettings.password;
                }
            }
            return {/*shareable*/ true, sharingSettings.expirationTimeMs, digest};
        }
    }

    return { /*shareable*/ false, std::chrono::milliseconds{}, std::optional<QString>{}};
}

template<typename Bookmark, typename T>
Bookmark bookmarkToApi(T&& oldBookmark, nx::Uuid serverId, bool includeDigest = false)
{
    NX_ASSERT(!oldBookmark.guid.isNull());
    NX_ASSERT(!serverId.isNull());

    Bookmark result;

    result.deviceId = std::forward<T>(oldBookmark).cameraId;
    result.name = std::forward<T>(oldBookmark).name;
    result.description = std::forward<T>(oldBookmark).description;
    result.startTimeMs = std::forward<T>(oldBookmark).startTimeMs;
    result.durationMs = std::forward<T>(oldBookmark).durationMs;
    result.tags = nx::toStdSet(std::forward<T>(oldBookmark).tags);
    result.creatorUserId = std::forward<T>(oldBookmark).creatorId;
    result.creationTimeMs = std::forward<T>(oldBookmark).creationTime();

    result.setIds(std::forward<T>(oldBookmark).guid, serverId);

    if constexpr (hasShare<Bookmark>::value)
    {
        if (std::forward<T>(oldBookmark).shareable())
        {
            auto share = api::BookmarkSharingSettings{
                .expirationTimeMs{std::forward<T>(oldBookmark).share.expirationTimeMs}};

            if (std::forward<T>(oldBookmark).share.digest)
            {
                if (includeDigest)
                    share.password = std::forward<T>(oldBookmark).share.digest.value();
                else
                    share.password = nx::utils::Url::kMaskedPassword;
            }

            result.share = std::move(share);
        }
    }

    return result;
}

template<typename Bookmark>
QnCameraBookmark bookmarkFromApi(Bookmark&& bookmark, bool digestPassword = true)
{
    QnCameraBookmark result;

    result.guid = std::move(bookmark.bookmarkId());
    result.cameraId = nx::Uuid(bookmark.deviceId);
    result.name = std::move(bookmark.name);
    result.description = std::move(bookmark.description);
    result.startTimeMs = std::move(bookmark.startTimeMs);
    result.durationMs = std::move(bookmark.durationMs);
    result.tags = nx::utils::toQSet(bookmark.tags);
    result.creatorId = bookmark.creatorUserId.value_or(nx::Uuid());
    result.creationTimeStampMs =
        bookmark.creationTimeMs.value_or(std::chrono::milliseconds::zero());
    result.share = shareableParams(bookmark, digestPassword);

    return result;
}

} // namespace nx::vms::common
