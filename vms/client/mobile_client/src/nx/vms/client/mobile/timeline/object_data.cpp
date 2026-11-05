// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "object_data.h"

#include <chrono>

#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <nx/utils/log/format.h>
#include <nx/vms/client/core/bookmarks/bookmark_constants.h>
#include <nx/vms/client/core/bookmarks/bookmark_utils.h>
#include <nx/vms/client/core/cross_system/cross_system_camera_resource.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/thumbnails/remote_async_image_provider.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <utils/common/synctime.h>

namespace nx::vms::client::mobile {
namespace timeline {

using namespace std::chrono;

QVariant ObjectData::getAttributes() const
{
    return QVariant::fromValue(attributes);
}

common::CameraBookmark toBookmark(const ObjectData& data, const QnResourcePtr& resource)
{
    if (const auto bookmark = get_if<common::CameraBookmark>(&data.source))
        return *bookmark;

    const auto camera = resource.objectCast<QnVirtualCameraResource>();
    if (!NX_ASSERT(camera))
        return {};

    const auto context = core::SystemContext::fromResource(camera);
    if (!NX_ASSERT(context && context->userWatcher()->user()))
        return {};

    constexpr qint64 kMinDurationMs = 8000;

    return common::CameraBookmark{
        .guid = nx::Uuid{}, //< Prefilled bookmark.
        .creatorId = context->userWatcher()->user()->getId(),
        .creationTimeStampMs = qnSyncTime->value(),
        .name = data.title,
        .description = core::bookmarks::bookmarkDescription(
            milliseconds(data.startTimeMs), camera, data.attributes),
        .startTimeMs = milliseconds(data.startTimeMs),
        .durationMs = milliseconds(std::max<qint64>(data.durationMs, kMinDurationMs)),
        .tags = {core::bookmarks::BookmarkConstants::objectBasedTagName()},
        .cameraId = camera->getId(),
    };
}

QString systemIdParameterName()
{
    return core::RemoteAsyncImageProvider::systemIdParameterName();
}

QString crossSystemId(const QnResourcePtr& resource)
{
    if (const auto crossSystemCamera = resource.objectCast<core::CrossSystemCameraResource>())
        return crossSystemCamera->systemId();

    return {};
}

QString makeImageRequest(const QnResourcePtr& resource,
    qint64 timestampMs,
    int resolution,
    const QList<std::pair<QString, QString>>& extraParams)
{
    static const QString kRequestTemplate =
        "ec2/cameraThumbnails?cameraId=%1&time=%2"
        "&width=%3&height=0&method=precise&imageFormat=jpg";

    static const QString kExtraParamTemplate = "&%1=%2";

    QString result =
        nx::format(kRequestTemplate, resource->getId().toSimpleString(), timestampMs, resolution);

    for (const auto& [key, value]: extraParams)
        result += nx::format(kExtraParamTemplate, key, value);

    if (const auto systemId = crossSystemId(resource); !systemId.isEmpty())
        result += nx::format(kExtraParamTemplate, systemIdParameterName(), systemId);

    return result;
}

} // namespace timeline
} // namespace nx::vms::client::mobile
