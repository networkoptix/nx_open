// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "utils.h"

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>

namespace {

const QString kOpenDoorPortName =
    QString::fromStdString(nx::reflect::toString(nx::vms::api::ExtendedCameraOutput::powerRelay));

} // namespace

namespace nx::vms::common {

bool checkIntercomCallPortExists(const QnVirtualCameraResourcePtr& camera)
{
    const auto portDescriptions = camera->ioPortDescriptions();
    return std::any_of(
        portDescriptions.begin(),
        portDescriptions.end(),
        [](const auto& portData)
        {
            return portData.outputName == kOpenDoorPortName;
        });
}

QnUuid calculateIntercomLayoutId(const QnResourcePtr& intercom)
{
    return calculateIntercomLayoutId(intercom->getId());
}

QnUuid calculateIntercomLayoutId(const QnUuid& intercomId)
{
    return guidFromArbitraryData(intercomId.toString() + "layout");
}

bool isIntercom(const QnResourcePtr& resource)
{
    if (!resource)
        return false;

    const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return false;

    return checkIntercomCallPortExists(camera);
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
