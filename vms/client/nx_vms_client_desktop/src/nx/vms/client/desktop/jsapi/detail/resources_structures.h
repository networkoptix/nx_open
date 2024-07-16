// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonValue>
#include <QtCore/QString>

#include <core/resource/resource_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>

#include "globals_structures.h"

namespace nx::vms::client::desktop::jsapi {

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

    /** Virtual camera Resource. */
    virtual_camera,

    /** Web page Resource. */
    web_page,

    /** Video file Resource. */
    local_video,

    /** Image file Resource. */
    local_image,

    /** Layout Resource. */
    layout
)

/**
 * @private
 */
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

    static Resource from(const QnResourcePtr& resource);
    static QList<Resource> from(const QnResourceList& resources);
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

/**
 * Resource parameter operation result.
 * @ingroup vms
 */
struct ParameterResult
{
    /** Error description. */
    Error error;

    /** Value, can be a string, an object or an array. */
    QJsonValue value = {};
};
NX_REFLECTION_INSTRUMENT(ParameterResult, (error)(value))

} // namespace nx::vms::client::desktop::jsapi
