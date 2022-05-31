// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/vms/common/resource/resource_descriptor.h>

namespace nx::vms::client::desktop {

/** Create resource descriptor for storing in layout item or passing to another client instance. */
nx::vms::common::ResourceDescriptor descriptor(const QnResourcePtr& resource);

/** Find Resource in a corresponding System Context. */
QnResourcePtr getResourceByDescriptor(const nx::vms::common::ResourceDescriptor& descriptor);

/** Whether Resource belongs to another cloud System. */
bool isCrossSystemResource(const nx::vms::common::ResourceDescriptor& descriptor);

} // namespace nx::vms::client::desktop
