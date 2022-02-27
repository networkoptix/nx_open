// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "group_descriptor_manager.h"

#include <nx/analytics/properties.h>
#include <nx/analytics/utils.h>

namespace nx::analytics {

using namespace nx::vms::api::analytics;

GroupDescriptorManager::GroupDescriptorManager(QObject* parent):
    base_type(parent),
    QnCommonModuleAware(parent)
{
}

std::optional<GroupDescriptor> GroupDescriptorManager::descriptor(
    const GroupId& groupId) const
{
    return fetchDescriptor<GroupDescriptor>(commonModule()->descriptorContainer(), groupId);
}

GroupDescriptorMap GroupDescriptorManager::descriptors(const std::set<GroupId>& groupIds) const
{
    return fetchDescriptors<GroupDescriptor>(commonModule()->descriptorContainer(), groupIds);
}

} // namespace nx::analytics
