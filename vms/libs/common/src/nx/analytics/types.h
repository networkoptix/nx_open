#pragma once

#include <nx/vms/api/analytics/descriptors.h>

#include <nx/analytics/replacement_merge_executor.h>
#include <nx/analytics/scoped_merge_executor.h>
#include <nx/analytics/device_descriptor_merge_executor.h>
#include <nx/analytics/multiresource_descriptor_container.h>
#include <nx/analytics/property_descriptor_storage_factory.h>

namespace nx::analytics {

using PluginId = QString;
using EngineId = QnUuid;
using GroupId = QString;
using EventTypeId = QString;
using ObjectTypeId = QString;
using ActionTypeId = QString;
using DeviceId = QnUuid;
using ManifestItemId = QString;

using PluginDescriptorMap = std::map<PluginId, nx::vms::api::analytics::PluginDescriptor>;
using EngineDescriptorMap = std::map<EngineId, nx::vms::api::analytics::EngineDescriptor>;
using GroupDescriptorMap = std::map<GroupId, nx::vms::api::analytics::GroupDescriptor>;
using EventTypeDescriptorMap = std::map<EventTypeId, nx::vms::api::analytics::EventTypeDescriptor>;
using ObjectTypeDescriptorMap = std::map<
    ObjectTypeId,
    nx::vms::api::analytics::ObjectTypeDescriptor>;

using ScopedEventTypeIds = std::map<EngineId, std::map<GroupId, std::set<EventTypeId>>>;
using ScopedObjectTypeIds = std::map<EngineId, std::map<GroupId, std::set<ObjectTypeId>>>;

using ActionTypeDescriptorMap = std::map<
    EngineId,
    std::map<ActionTypeId, nx::vms::api::analytics::ActionTypeDescriptor>>;

using DeviceDescriptorMap = std::map<DeviceId, nx::vms::api::analytics::DeviceDescriptor>;

using PluginDescriptorContainer = MultiresourceDescriptorContainer<
    PropertyDescriptorStorageFactory<nx::vms::api::analytics::PluginDescriptor, PluginId>,
    ReplacementMergeExecutor<nx::vms::api::analytics::PluginDescriptor>>;

using EngineDescriptorContainer = MultiresourceDescriptorContainer<
    PropertyDescriptorStorageFactory<nx::vms::api::analytics::EngineDescriptor, EngineId>,
    ReplacementMergeExecutor<nx::vms::api::analytics::EngineDescriptor>>;

using GroupDescriptorContainer = MultiresourceDescriptorContainer<
    PropertyDescriptorStorageFactory<nx::vms::api::analytics::GroupDescriptor, GroupId>,
    ScopedMergeExecutor<nx::vms::api::analytics::GroupDescriptor>>;

using EventTypeDescriptorContainer = MultiresourceDescriptorContainer<
    PropertyDescriptorStorageFactory<nx::vms::api::analytics::EventTypeDescriptor, EventTypeId>,
    ScopedMergeExecutor<nx::vms::api::analytics::EventTypeDescriptor>>;

using ObjectTypeDescriptorContainer = MultiresourceDescriptorContainer<
    PropertyDescriptorStorageFactory<nx::vms::api::analytics::ObjectTypeDescriptor, ObjectTypeId>,
    ScopedMergeExecutor<nx::vms::api::analytics::ObjectTypeDescriptor>>;

using ActionTypeDescriptorContainer = MultiresourceDescriptorContainer<
    PropertyDescriptorStorageFactory<
        nx::vms::api::analytics::ActionTypeDescriptor,
        EngineId,
        ActionTypeId>,
    ReplacementMergeExecutor<nx::vms::api::analytics::ActionTypeDescriptor>>;

using DeviceDescriptorContainer = MultiresourceDescriptorContainer<
    PropertyDescriptorStorageFactory<nx::vms::api::analytics::DeviceDescriptor, DeviceId>,
    DeviceDescriptorMergeExecutor>;

} // namespace nx::analytics
