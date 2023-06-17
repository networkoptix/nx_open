// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cross_system_layout_resource.h"

#include "cross_system_layout_data.h"

namespace nx::vms::client::desktop {

CrossSystemLayoutResource::CrossSystemLayoutResource()
{
    addFlags(Qn::cross_system);
    forceUsingLocalProperties();
}

void CrossSystemLayoutResource::update(const CrossSystemLayoutData& layoutData)
{
    if (!NX_ASSERT(layoutData.id == this->getId()))
        return;

    CrossSystemLayoutResourcePtr copy(new CrossSystemLayoutResource());
    fromDataToResource(layoutData, copy);
    copy->setFlags(flags()); //< Do not update current resource flags.
    QnResource::update(copy);
}

LayoutResourcePtr CrossSystemLayoutResource::createClonedInstance() const
{
    return LayoutResourcePtr(new CrossSystemLayoutResource());
}

} // namespace nx::vms::client::desktop
