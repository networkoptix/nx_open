// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resources_structures.h"

#include <core/resource/resource.h>
#include <nx/vms/client/desktop/system_context.h>

namespace nx::vms::client::desktop::jsapi {
namespace detail {

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

} // namespace detail

ResourceUniqueId::ResourceUniqueId(const QString& str)
{
    QStringList ids = str.split(".");
    NX_ASSERT(ids.size() == 1 || ids.size() == 2);
    const bool isTwoComponentFormat = ids.size() == 2;
    if (isTwoComponentFormat)
    {
        localSystemId = QUuid(ids[0]);
        id = QUuid(ids[1]);
    }
    else if (!ids.empty())
    {
        id = QUuid(ids[0]);
    }
}

ResourceUniqueId::operator QString() const
{
    return nx::format("%1.%2", localSystemId, id);
}

bool ResourceUniqueId::isNull() const
{
    return id.isNull() && localSystemId.isNull();
}

ResourceUniqueId ResourceUniqueId::from(const QnResourcePtr& resource)
{
    ResourceUniqueId result;
    result.id = resource->getId();
    result.localSystemId = SystemContext::fromResource(resource)->localSystemId();
    return result;
}

std::string toString(const ResourceUniqueId& id)
{
    return QString(id).toStdString();
}

bool fromString(const std::string& str, ResourceUniqueId* id)
{
    const ResourceUniqueId result{QString::fromStdString(str)};
    if (result.isNull())
        return false;

    *id = result;
    return true;
}

Resource Resource::from(const QnResourcePtr& resource)
{
    if (!resource)
        return Resource();

    Resource result;
    result.id = ResourceUniqueId::from(resource);
    result.name = resource->getName();
    result.logicalId = resource->logicalId();
    result.type = detail::resourceType(resource);
    return result;
}

QList<Resource> Resource::from(const QnResourceList& resources)
{
    QList<Resource> result;
    for (const auto& resource: resources)
        result.append(Resource::from(resource));
    return result;
}

} // namespace nx::vms::client::desktop::jsapi::detail
