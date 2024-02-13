// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <core/resource/resource_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>

#include "globals_structures.h"

namespace nx::vms::client::desktop::jsapi::detail {

/**
 * @ingroup vms
 */
NX_REFLECTION_ENUM_CLASS(ResourceType,
    /** Invalid Resource type. */
    undefined,

    /** Server Resource. */
    server,

    /** Real camera Resource. */
    camera,

    /** IO module Resource. */
    io_module,

    /** Some streaming Resource like an RTSP sources. */
    virtual_camera,

    /** Web page Resource. */
    web_page,

    /** Video file Resource. */
    local_video,

    /** Image file Resource. */
    local_image
)

struct ResourceUniqueId
{
    nx::Uuid id;
    nx::Uuid localSystemId;

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

/**
 * Resource description.
 * @ingroup vms
 */
struct Resource
{
    /** Unique identifier. */
    ResourceUniqueId id;

    QString name;
    ResourceType type = ResourceType::undefined;
    int logicalId = 0;

    /** @privatesection */

    using List = QList<Resource>;
    static Resource from(const QnResourcePtr& resource);
    static List from(const QnResourceList& resources);
};
NX_REFLECTION_INSTRUMENT(Resource, (id)(name)(type)(logicalId))

/**
 * Resource operation result.
 * @ingroup vms
 */
struct ResourceResult
{
    /** Error description. */
    Error error;

    /** Resource description. */
    Resource resource;
};
NX_REFLECTION_INSTRUMENT(ResourceResult, (error)(resource))

/** Check if a resource with the specified type may have media stream (at least theoretically). */
bool hasMediaStream(const ResourceType type);

ResourceType resourceType(const QnResourcePtr& resource);

} // namespace nx::vms::client::desktop::jsapi::detail
