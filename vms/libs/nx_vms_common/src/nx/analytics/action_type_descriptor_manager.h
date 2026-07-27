// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/vms/api/analytics/descriptors.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>
#include <nx/vms/api/analytics/engine_manifest.h>

class QnRuntimeInfoManager;

namespace nx::analytics {

class NX_VMS_COMMON_API ActionTypeDescriptorManager final
{
public:
    ActionTypeDescriptorManager(QnResourcePool* resourcePool, QnRuntimeInfoManager* runtimeInfoManager);

    std::optional<nx::vms::api::analytics::ActionTypeDescriptor> descriptor(
        const nx::vms::api::analytics::ActionTypeId& actionTypeId) const;

    nx::vms::api::analytics::ActionTypeDescriptorMap availableObjectActionTypeDescriptors(
        const nx::vms::api::analytics::ObjectTypeId& objectTypeId,
        const QnVirtualCameraResourcePtr& device) const;

private:
    QnResourcePool* const m_resourcePool;
    QnRuntimeInfoManager* const m_runtimeInfoManager;
};

} // namespace nx::analytics
