// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "roi_camera_thumbnail.h"

#include <QtQml/QtQml>

#include <client_core/client_core_module.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

namespace nx::vms::client::desktop {

struct RoiCameraThumbnail::Private
{
    RoiCameraThumbnail* const q;
    QnUuid engineId;

    void updateStream()
    {
        const auto camera = q->resource().objectCast<QnVirtualCameraResource>();
        if (!camera)
            return;

        const nx::vms::api::StreamIndex analyzedStreamIndex = camera->analyzedStreamIndex(engineId);
        q->setStream(analyzedStreamIndex == nx::vms::api::StreamIndex::primary
            ? CameraStream::primary
            : CameraStream::secondary);
    }
};

RoiCameraThumbnail::RoiCameraThumbnail(QObject* parent):
    base_type(parent),
    d(new Private{this})
{
    connect(this, &AbstractResourceThumbnail::resourceChanged, this,
        [this]() { d->updateStream(); });
}

RoiCameraThumbnail::~RoiCameraThumbnail()
{
    // Required here for forward-declared scoped pointer destruction.
}

QnUuid RoiCameraThumbnail::cameraId() const
{
    if (const auto camera = resource().objectCast<QnVirtualCameraResource>())
        return camera->getId();

    return {};
}

void RoiCameraThumbnail::setCameraId(const QnUuid& value)
{
    setResource(qnClientCoreModule->commonModule()->resourcePool()
        ->getResourceById<QnVirtualCameraResource>(value));
}

QnUuid RoiCameraThumbnail::engineId() const
{
    return d->engineId;
}

void RoiCameraThumbnail::setEngineId(const QnUuid& value)
{
    if (d->engineId == value)
        return;

    d->engineId = value;
    emit engineIdChanged();

    d->updateStream();
}

void RoiCameraThumbnail::registerQmlType()
{
    qmlRegisterType<RoiCameraThumbnail>("nx.vms.client.desktop", 1, 0,
        "RoiCameraThumbnail");
}

} // namespace nx::vms::client::desktop
