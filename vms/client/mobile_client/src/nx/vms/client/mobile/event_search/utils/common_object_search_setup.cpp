// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "common_object_search_setup.h"

#include <QtQml/QtQml>

#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/core/system_context.h>

namespace nx::vms::client::mobile {

struct CommonObjectSearchSetup::Private
{
};

void CommonObjectSearchSetup::registerQmlType()
{
    qmlRegisterUncreatableType<CommonObjectSearchSetup>("Nx.Mobile", 1, 0,
        "CommonObjectSearchSetup", "Can't instantinate CommonObjectSearchSetup object");
}

CommonObjectSearchSetup::CommonObjectSearchSetup(
    core::SystemContext* context,
    QObject* parent)
    :
    base_type(parent),
    d(new Private())
{
    setContext(context);
}

CommonObjectSearchSetup::~CommonObjectSearchSetup()
{
}

bool CommonObjectSearchSetup::selectCameras(UuidSet& /*selectedCameras*/)
{
    return false;
}

QnVirtualCameraResourcePtr CommonObjectSearchSetup::currentResource() const
{
    return {};
}

QnVirtualCameraResourceSet CommonObjectSearchSetup::currentLayoutCameras() const
{
    return {};
}

void CommonObjectSearchSetup::clearTimelineSelection() const
{
}

} // namespace nx::vms::client::mobile
