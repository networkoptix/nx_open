// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/camera_bookmark.h>
#include <nx/utils/member_detector.h>
#include <nx/utils/qt_helpers.h>
#include <nx/utils/std_helpers.h>
#include <nx/vms/api/data/bookmark_models.h>

namespace nx::vms::common {

NX_UTILS_DECLARE_FIELD_DETECTOR_SIMPLE(hasShare, share);

template<typename BookmarkWithRule>
common::BookmarkShareableParams shareableParams(
    const BookmarkWithRule& data, bool digestPassword = true)
{
    if constexpr (hasShare<BookmarkWithRule>::value)
    {
        if (data.share && std::holds_alternative<api::BookmarkSharingSettings>(data.share.value()))
        {
            QString id;
            if constexpr (std::is_same_v<BookmarkWithRule, api::BookmarkWithRuleV1>)
                id = NX_FMT("%1_%2", data.id->toSimpleString(), data.serverId->toSimpleString());
            else
                id = data.id;

            auto& sharingSettings = std::get<api::BookmarkSharingSettings>(data.share.value());

            std::optional<QString> digest;
            if (!sharingSettings.password.isEmpty())
            {
                if (digestPassword)
                {
                    digest = api::BookmarkProtection::getDigest(
                        id, sharingSettings.password);
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

template<typename Bookmark>
Bookmark bookmarkToApi(
    QnCameraBookmark&& oldBookmark, nx::Uuid serverId, bool includeDigest = false)
{
    NX_ASSERT(!oldBookmark.guid.isNull());
    NX_ASSERT(!serverId.isNull());

    Bookmark result;

    result.deviceId = std::move(oldBookmark.cameraId),
    result.name = std::move(oldBookmark.name),
    result.description = std::move(oldBookmark.description),
    result.startTimeMs = std::move(oldBookmark.startTimeMs),
    result.durationMs = std::move(oldBookmark.durationMs),
    result.tags = nx::toStdSet(oldBookmark.tags),
    result.creatorUserId = std::move(oldBookmark.creatorId),
    result.creationTimeMs = std::move(oldBookmark.creationTime()),

    result.setIds(oldBookmark.guid, serverId);

    if constexpr (hasShare<Bookmark>::value)
    {
        if (oldBookmark.shareable())
        {
            auto share = api::BookmarkSharingSettings{
                .expirationTimeMs{oldBookmark.share.expirationTimeMs}};

            if (includeDigest)
            {
                share.password =
                    oldBookmark.share.digest ? oldBookmark.share.digest.value() : QString{};
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
