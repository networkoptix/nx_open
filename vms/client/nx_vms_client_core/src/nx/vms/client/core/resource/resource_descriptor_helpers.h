// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/vms/common/resource/resource_descriptor.h>

namespace nx::vms::client::core {

/**
 * Create resource descriptor for storing in layout item or passing to another client instance.
 * @param forceCloud Generates cloud path even for resources from the current system.
 */
nx::vms::common::ResourceDescriptor NX_VMS_CLIENT_CORE_API descriptor(
    const QnResourcePtr& resource,
    bool forceCloud = false);

/** Find Resource in a corresponding System Context. */
QnResourcePtr NX_VMS_CLIENT_CORE_API getResourceByDescriptor(
    const nx::vms::common::ResourceDescriptor& descriptor);

/** Whether Resource belongs to another cloud System. */
bool NX_VMS_CLIENT_CORE_API isCrossSystemResource(
    const nx::vms::common::ResourceDescriptor& descriptor);

/** Get System Id of the Cloud System this cross-system Resource belongs to. */
QString NX_VMS_CLIENT_CORE_API crossSystemResourceSystemId(
    const nx::vms::common::ResourceDescriptor& descriptor);

/** Desktop camera resource descriptor always contains specific path value. */
bool NX_VMS_CLIENT_CORE_API isDesktopCameraResource(
    const nx::vms::common::ResourceDescriptor& descriptor);

/** Desktop camera resource descriptor always contains physical id in its path. */
QString NX_VMS_CLIENT_CORE_API getDesktopCameraPhysicalId(
    const nx::vms::common::ResourceDescriptor& descriptor);

/** System Id placeholder for resources, not bound to any System (e.g. cloud layouts). */
QString NX_VMS_CLIENT_CORE_API genericCloudSystemId();

} // namespace nx::vms::client::core
