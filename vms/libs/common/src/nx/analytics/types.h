#pragma once

#include <nx/vms/api/analytics/descriptors.h>

namespace nx::analytics {

using PluginId = QString;
using EngineId = QnUuid;
using GroupId = QString;
using EventTypeId = QString;
using ObjectTypeId = QString;
using ActionTypeId = QString;

using PluginDescriptorMap = std::map<PluginId, nx::vms::api::analytics::PluginDescriptor>;
using EngineDescriptorMap = std::map<EngineId, nx::vms::api::analytics::EngineDescriptor>;
using GroupDescriptorMap = std::map<GroupId, nx::vms::api::analytics::GroupDescriptor>;
using EventTypeDescriptorMap = std::map<EventTypeId, nx::vms::api::analytics::EventTypeDescriptor>;
using ObjectTypeDescriptorMap = std::map<
    ObjectTypeId,
    nx::vms::api::analytics::ObjectTypeDescriptor>;

using ActionTypeDescriptorMap = MapHelper::NestedMap<
    std::map,
    EngineId,
    ActionTypeId,
    nx::vms::api::analytics::ActionTypeDescriptor>;

} // namespace nx::analytics
