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
};

RoiCameraThumbnail::RoiCameraThumbnail(QObject* parent):
    base_type(parent),
    d(new Private{this})
{
}

RoiCameraThumbnail::~RoiCameraThumbnail()
{
    // Required here for forward-declared scoped pointer destruction.
}

nx::Uuid RoiCameraThumbnail::cameraId() const
{
    if (const auto camera = resource().objectCast<QnVirtualCameraResource>())
        return camera->getId();

    return {};
}

void RoiCameraThumbnail::setCameraId(const nx::Uuid& value)
{
    setResource(qnClientCoreModule->resourcePool()
        ->getResourceById<QnVirtualCameraResource>(value));
}

void RoiCameraThumbnail::registerQmlType()
{
    qmlRegisterType<RoiCameraThumbnail>("nx.vms.client.desktop", 1, 0,
        "RoiCameraThumbnail");
}

} // namespace nx::vms::client::desktop
