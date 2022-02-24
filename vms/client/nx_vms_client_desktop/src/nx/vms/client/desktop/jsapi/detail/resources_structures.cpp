// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resources_structures.h"

#include <core/resource/resource.h>

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
    if (flags.testFlag(Qn::server) && !flags.testFlag(Qn::fake_server))
        return ResourceType::server;
    if (flags.testFlag(Qn::live_cam))
        return ResourceType::camera;

    return detail::ResourceType::undefined;
}

bool hasMediaStream(const ResourceType type)
{
    return type == detail::ResourceType::camera
        || type == detail::ResourceType::virtual_camera
        || type == detail::ResourceType::io_module
        || type == detail::ResourceType::local_video;
}

Resource Resource::from(const QnResourcePtr& resource)
{
    if (!resource)
        return Resource();

    Resource result;
    result.id = resource->getId().toQUuid();
    result.name = resource->getName();
    result.logicalId = resource->logicalId();
    result.type = resourceType(resource);
    return result;
}

Resource::List Resource::from(const QnResourceList& resources)
{
    List result;
    for (const auto& resource: resources)
        result.append(Resource::from(resource));
    return result;
}

} // namespace nx::vms::client::desktop::jsapi::detail
