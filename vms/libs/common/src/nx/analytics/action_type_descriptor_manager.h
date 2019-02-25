#pragma once

#include <nx/analytics/types.h>

#include <nx/vms/api/analytics/descriptors.h>
#include <nx/vms/api/analytics/engine_manifest.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>

#include <common/common_module_aware.h>
#include <core/resource/resource_fwd.h>

namespace nx::analytics {

class ActionTypeDescriptorManager : public /*mixin*/ QnCommonModuleAware
{
    using base_type = QnCommonModuleAware;

public:
    ActionTypeDescriptorManager(QnCommonModule* commonModule);

    std::optional<nx::vms::api::analytics::ActionTypeDescriptor> descriptor(
        const ActionTypeId& actionTypeId) const;

    ActionTypeDescriptorMap availableObjectActionTypeDescriptors(
        const ObjectTypeId& objectTypeId,
        const QnVirtualCameraResourcePtr& device) const;
};

} // namespace nx::analytics
