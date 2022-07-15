// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cross_system_layout_resource.h"

#include <nx_ec/data/api_conversion_functions.h>
#include <nx/vms/api/data/layout_data.h>

namespace nx::vms::client::desktop {

CrossSystemLayoutResource::CrossSystemLayoutResource()
{
}

void CrossSystemLayoutResource::update(const nx::vms::api::LayoutData& layoutData)
{
    QnLayoutResourcePtr copy(new QnLayoutResource());
    ec2::fromApiToResource(layoutData, copy);
    QnResource::update(copy);
}

} // namespace nx::vms::client::desktop
