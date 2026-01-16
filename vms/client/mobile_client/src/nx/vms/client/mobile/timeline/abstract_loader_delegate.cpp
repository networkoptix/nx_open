// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_loader_delegate.h"

#include <nx/utils/log/format.h>
#include <nx/vms/client/core/cross_system/cross_system_camera_resource.h>
#include <nx/vms/client/core/thumbnails/remote_async_image_provider.h>

namespace nx::vms::client::mobile {
namespace timeline {

QString AbstractLoaderDelegate::crossSystemId(const QnResourcePtr& resource)
{
    if (const auto crossSystemCamera = resource.objectCast<core::CrossSystemCameraResource>())
        return crossSystemCamera->systemId();

    return {};
}

QString AbstractLoaderDelegate::systemIdParameterName()
{
    return core::RemoteAsyncImageProvider::systemIdParameterName();
}

QString AbstractLoaderDelegate::makeImageRequest(const QnResourcePtr& resource, qint64 timestampMs,
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

} // namespace timeline
} // namespace nx::vms::client::mobile
