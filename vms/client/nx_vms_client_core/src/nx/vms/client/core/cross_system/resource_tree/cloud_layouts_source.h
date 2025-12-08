// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/core/resource/resource_fwd.h>
#include <nx/vms/client/core/resource_views/entity_resource_tree/resource_source/abstract_resource_source.h>

namespace nx::vms::client::core {
namespace entity_resource_tree {

class NX_VMS_CLIENT_CORE_API CloudLayoutsSource: public AbstractResourceSource
{
public:
    CloudLayoutsSource();
    virtual QVector<QnResourcePtr> getResources() override;
};

} // namespace entity_resource_tree
} // namespace nx::vms::client::core
