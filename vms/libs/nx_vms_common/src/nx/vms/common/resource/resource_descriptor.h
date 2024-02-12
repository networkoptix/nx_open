// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>

namespace nx::vms::common {

/**
 * Description of the single resource, used to store it on the Layouts, search resource instance
 * in the different System Contexts, pass it to another client window, etc.
 */
struct ResourceDescriptor
{
    /** Resource id in the System Context. */
    nx::Uuid id;

    /**
     * Custom resource path. Contains different values depending on resource types:
     * - Path to the file for the local media resources.
     * - Url for web pages.
     * - Url of the special form `cloud://{NULL UUID}.{id}` for the cloud layouts.
     * - Url of the special form `cloud://{system id}.{id}` for the cross-system resources.
     */
    QString path;

    /**
     * Name of the resource. Usable when it's needed to create resource placeholder ('thumb
     * resource') while actual resource instance is not initialized yet.
     */
    QString name;

    bool operator==(const ResourceDescriptor&) const = default;
};

#define ResourceDescriptor_Fields (id)(path)(name)
NX_REFLECTION_INSTRUMENT(ResourceDescriptor, ResourceDescriptor_Fields)

} // namespace nx::vms::common
