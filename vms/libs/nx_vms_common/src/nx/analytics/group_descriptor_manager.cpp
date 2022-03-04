// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "group_descriptor_manager.h"

#include <nx/analytics/properties.h>
#include <nx/analytics/utils.h>

namespace nx::analytics {

using namespace nx::vms::api::analytics;

GroupDescriptorManager::GroupDescriptorManager(
    taxonomy::DescriptorContainer* taxonomyDescriptorContainer,
    QObject* parent)
    :
    base_type(parent),
    m_taxonomyDescriptorContainer(taxonomyDescriptorContainer)
{
}

std::optional<GroupDescriptor> GroupDescriptorManager::descriptor(
    const GroupId& groupId) const
{
    return fetchDescriptor<GroupDescriptor>(m_taxonomyDescriptorContainer, groupId);
}

GroupDescriptorMap GroupDescriptorManager::descriptors(const std::set<GroupId>& groupIds) const
{
    return fetchDescriptors<GroupDescriptor>(m_taxonomyDescriptorContainer, groupIds);
}

} // namespace nx::analytics
