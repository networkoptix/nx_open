// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "helpers.h"

#include <core/resource/resource.h>
#include <nx/vms/client/core/resource/unified_resource_pool.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/resource/resource_access_manager.h>
#include <nx/vms/client/desktop/system_context.h>

namespace nx::vms::client::desktop::jsapi::detail {

ResourceType resourceType(const QnResourcePtr& resource)
{
    if (!resource)
        return ResourceType::undefined;

    const auto flags = resource->flags();
    if (flags.testFlag(Qn::desktop_camera))
        return ResourceType::undefined;

    if (flags.testFlag(Qn::web_page))
        return ResourceType::web_page;
    if (flags.testFlag(Qn::virtual_camera))
        return ResourceType::virtual_camera;
    if (flags.testFlag(Qn::io_module))
        return ResourceType::io_module;
    if (flags.testFlag(Qn::local_image))
        return ResourceType::local_image;
    if (flags.testFlag(Qn::local_video))
        return ResourceType::local_video;
    if (flags.testFlag(Qn::server))
        return ResourceType::server;
    if (flags.testFlag(Qn::live_cam))
        return ResourceType::camera;
    if (flags.testFlag(Qn::layout))
        return ResourceType::layout;

    return ResourceType::undefined;
}

bool hasMediaStream(const ResourceType type)
{
    return type == ResourceType::camera
        || type == ResourceType::virtual_camera
        || type == ResourceType::io_module
        || type == ResourceType::local_video;
}

bool isResourceAvailable(const QnResourcePtr& resource)
{
    if (!resource)
        return false;

    switch (detail::resourceType(resource))
    {
        case ResourceType::layout:
        {
            return !resource->hasFlags(Qn::ResourceFlag::local) //< Ignore unsaved layouts.
                && ResourceAccessManager::hasPermissions(resource, Qn::ReadPermission);
        }

        case ResourceType::undefined:
            return false;

        default:
            return ResourceAccessManager::hasPermissions(resource, Qn::ViewContentPermission);
    }
}

QnResourcePtr getResourceIfAvailable(const ResourceUniqueId& resourceId)
{
    const core::UnifiedResourcePool* pool = appContext()->unifiedResourcePool();
    const auto resource = pool->resource(
        resourceId.id,
        resourceId.localSystemId.isNull()
            ? appContext()->currentSystemContext()->localSystemId()
            : resourceId.localSystemId);

    return isResourceAvailable(resource) ? resource : QnResourcePtr{};
}

} // namespace nx::vms::client::desktop::jsapi::detail
