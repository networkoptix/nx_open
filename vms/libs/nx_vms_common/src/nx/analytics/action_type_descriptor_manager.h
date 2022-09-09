// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/vms/api/analytics/descriptors.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>
#include <nx/vms/api/analytics/engine_manifest.h>
#include <nx/vms/common/system_context_aware.h>

namespace nx::analytics {

class NX_VMS_COMMON_API ActionTypeDescriptorManager: public nx::vms::common::SystemContextAware
{
public:
    ActionTypeDescriptorManager(nx::vms::common::SystemContext* systemContext);

    std::optional<nx::vms::api::analytics::ActionTypeDescriptor> descriptor(
        const nx::vms::api::analytics::ActionTypeId& actionTypeId) const;

    nx::vms::api::analytics::ActionTypeDescriptorMap availableObjectActionTypeDescriptors(
        const nx::vms::api::analytics::ObjectTypeId& objectTypeId,
        const QnVirtualCameraResourcePtr& device) const;
};

} // namespace nx::analytics
