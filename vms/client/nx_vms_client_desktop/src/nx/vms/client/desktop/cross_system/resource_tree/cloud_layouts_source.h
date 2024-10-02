// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/resource/resource_fwd.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_source/abstract_resource_source.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

class CloudLayoutsSource: public AbstractResourceSource
{
public:
    CloudLayoutsSource();
    virtual QVector<QnResourcePtr> getResources() override;

private:
    void processLocalLayout(const QnResourcePtr& resource);
    void listenLocalLayout(const CrossSystemLayoutResourcePtr& layout);
};

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
