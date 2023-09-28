// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <core/resource/resource_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>

#include "globals_structures.h"

namespace nx::vms::client::desktop::jsapi::detail {

/** Represent type of the resource. */
NX_REFLECTION_ENUM_CLASS(ResourceType,
    undefined,
    server,
    camera,
    io_module,
    virtual_camera,
    web_page,
    local_video,
    local_image)

/** Type of the specified resource. */
ResourceType resourceType(const QnResourcePtr& resource);

struct ResourceUniqueId
{
    QnUuid id;
    QnUuid systemId;

    ResourceUniqueId() = default;
    ResourceUniqueId(const QString& id);
    operator QString() const;
    bool isNull() const;

    static ResourceUniqueId from(const QnResourcePtr& resource);
};
NX_REFLECTION_TAG_TYPE(ResourceUniqueId, useStringConversionForSerialization)

// Functions for serialization and deserialization.
std::string toString(const ResourceUniqueId& id);
bool fromString(const std::string& str, ResourceUniqueId* id);

/** Resource description. */
struct Resource
{
    ResourceUniqueId id;
    QString name;
    ResourceType type = ResourceType::undefined;
    int logicalId = 0;

    using List = QList<Resource>;
    static Resource from(const QnResourcePtr& resource);
    static List from(const QnResourceList& resources);
};
NX_REFLECTION_INSTRUMENT(Resource, (id)(name)(type)(logicalId))

/** Represent result of resource 'get' operation. */
struct ResourceResult
{
    Error error;
    Resource resource;
};
NX_REFLECTION_INSTRUMENT(ResourceResult, (error)(resource))

/** Check if a resource with the specified type may have media stream (at least theoretically). */
bool hasMediaStream(const ResourceType type);

} // namespace nx::vms::client::desktop::jsapi::detail
