// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource.h>
#include <nx/vms/client/core/resource_views/entity_resource_tree/item/core_resource_item.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

class ResourceItem: public core::entity_resource_tree::CoreResourceItem
{
    using base_type = core::entity_resource_tree::CoreResourceItem;
public:
    ResourceItem(const QnResourcePtr& resource);
    virtual QVariant data(int role) const override;

protected:
    virtual void initResourceIconKeyNotifications() const override;

private:
    QVariant helpTopic() const;

private:
    const QVariant m_helpTopic;
};

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
