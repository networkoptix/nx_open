// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/core/resource_views/entity_item_model/item/abstract_item.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

class CloudCrossSystemCameraDecorator: public core::entity_item_model::AbstractItem
{
    using base_type = core::entity_item_model::AbstractItem;

public:
    CloudCrossSystemCameraDecorator(core::entity_item_model::AbstractItemPtr sourceItem);

    virtual QVariant data(int role) const override;
    virtual Qt::ItemFlags flags() const override;

private:
    core::entity_item_model::AbstractItemPtr m_sourceItem;
};

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
