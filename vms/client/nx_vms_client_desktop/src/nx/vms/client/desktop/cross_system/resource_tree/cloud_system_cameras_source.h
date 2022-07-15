// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/unique_key_source.h>

namespace nx::vms::client::desktop {

namespace entity_resource_tree {

class CloudSystemCamerasSource: public entity_item_model::UniqueResourceSource
{
public:
    CloudSystemCamerasSource(const QString& systemId);
    ~CloudSystemCamerasSource();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
