// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_object_data.h"

#include <chrono>

#include <QtQml/QtQml>

#include <core/resource/camera_bookmark.h>
#include <core/resource/media_resource.h>
#include <core/resource/user_resource.h>
#include <nx/utils/log/format.h>
#include <nx/vms/client/core/analytics/analytics_attribute_helper.h>
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

common::CameraBookmark AbstractObjectData::convertToBookmark() const
{
    const auto camera = resource().dynamicCast<QnMediaResource>();
    if (!NX_ASSERT(camera))
        return {};

    const auto context =
        camera->systemContext() ? camera->systemContext()->as<core::SystemContext>() : nullptr;

    if (!NX_ASSERT(context && context->userWatcher()->user()))
        return {};

    constexpr qint64 kMinDurationMs = 8000;

    return common::CameraBookmark {
        .guid = nx::Uuid{}, //< Prefilled bookmark.
        .creatorId = context->userWatcher()->user()->getId(),
        .creationTimeStampMs = qnSyncTime->value(),
        .name = title(),
        .description = core::bookmarks::bookmarkDescription(milliseconds(startTimeMs()), camera,
            attributes().value<core::analytics::AttributeList>()),
        .startTimeMs = milliseconds(startTimeMs()),
        .durationMs = milliseconds(std::max<qint64>(durationMs(), kMinDurationMs)),
        .tags = { core::bookmarks::BookmarkConstants::objectBasedTagName() },
        .cameraId = camera->getId(),
    };
}

QnResource* AbstractObjectData::getResource() const
{
    return resource().get();
}

void AbstractObjectData::registerQmlType()
{
    qmlRegisterUncreatableType<AbstractObjectData>("nx.vms.client.mobile.timeline", 1, 0,
        "AbstractObjectData", "ObjectData is not a creatable type");
}

QString AbstractObjectData::systemIdParameterName()
{
    return core::RemoteAsyncImageProvider::systemIdParameterName();
}

QString AbstractObjectData::crossSystemId(const QnResourcePtr& resource)
{
    if (const auto crossSystemCamera = resource.objectCast<core::CrossSystemCameraResource>())
        return crossSystemCamera->systemId();

    return {};
}

QString AbstractObjectData::makeImageRequest(const QnResourcePtr& resource, qint64 timestampMs,
    int resolution, const QList<std::pair<QString, QString>>& extraParams)
{
    static const QString kRequestTemplate = "ec2/cameraThumbnails?cameraId=%1&time=%2"
        "&width=%3&height=0&method=precise&imageFormat=jpg";

    static const QString kExtraParamTemplate = "&%1=%2";

    QString result = nx::format(kRequestTemplate,
        resource->getId().toSimpleString(), timestampMs, resolution);

    for (const auto& [key, value]: extraParams)
        result += nx::format(kExtraParamTemplate, key, value);

    if (const auto systemId = crossSystemId(resource); !systemId.isEmpty())
        result += nx::format(kExtraParamTemplate, systemIdParameterName(), systemId);

    return result;
}

QList<AbstractObjectData*> MultiObjectData::getPerObjectData() const
{
    QList<AbstractObjectData*> result;
    result.reserve(perObjectData.size());
    for (const auto& data: perObjectData)
        result.push_back(data.get());

    return result;
}

} // namespace timeline
} // namespace nx::vms::client::mobile
