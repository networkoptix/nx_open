// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cross_system_layout_resource.h"

#include <nx_ec/data/api_conversion_functions.h>
#include <nx/vms/api/data/layout_data.h>

namespace nx::vms::client::desktop {

CrossSystemLayoutResource::CrossSystemLayoutResource()
{
    addFlags(Qn::cross_system);
}

void CrossSystemLayoutResource::update(const nx::vms::api::LayoutData& layoutData)
{
    QnLayoutResourcePtr copy(new CrossSystemLayoutResource());
    copy->setFlags(flags()); //< Do not update current resource flags.
    ec2::fromApiToResource(layoutData, copy);
    QnResource::update(copy);
}

LayoutResourcePtr CrossSystemLayoutResource::create() const
{
    return LayoutResourcePtr(new CrossSystemLayoutResource());
}

} // namespace nx::vms::client::desktop
