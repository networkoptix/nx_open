// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "utils.h"

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>

namespace {

bool isIntercom(const QnResourcePtr& resource)
{
    const auto camera = resource.objectCast<QnVirtualCameraResource>();
    return camera && camera->isIntercom();
}

} // namespace

namespace nx::vms::common {

nx::Uuid calculateIntercomLayoutId(const QnResourcePtr& intercom)
{
    return calculateIntercomLayoutId(intercom->getId());
}

nx::Uuid calculateIntercomLayoutId(const nx::Uuid& intercomId)
{
    return guidFromArbitraryData(intercomId.toString() + "layout");
}

nx::Uuid calculateIntercomItemId(const QnResourcePtr& intercom)
{
    return guidFromArbitraryData(intercom->getId().toString() + "item");
}

bool isIntercomOnIntercomLayout(
    const QnResourcePtr& resource,
    const QnLayoutResourcePtr& layoutResource)
{
    if (!resource || !layoutResource)
        return false;

    if (calculateIntercomLayoutId(resource) != layoutResource->getId())
        return false;

    if (!layoutResource.dynamicCast<QnLayoutResource>())
        return false;

    return isIntercom(resource);
}

bool isIntercomLayout(const QnResourcePtr& layoutResource)
{
    if (!layoutResource || !layoutResource->hasFlags(Qn::layout))
        return false;

    return isIntercom(layoutResource->getParentResource());
}

} // namespace nx::vms::common
