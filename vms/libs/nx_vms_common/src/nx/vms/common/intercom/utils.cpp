// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "utils.h"

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>

namespace {

const QString kOpenDoorPortName =
    QString::fromStdString(nx::reflect::toString(nx::vms::api::ExtendedCameraOutput::powerRelay));

bool checkIntercomCallPortExists(const QnResourcePtr& resource)
{
    const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return false;

    const auto portDescriptions = camera->ioPortDescriptions();
    const auto portIter = std::find_if(portDescriptions.begin(), portDescriptions.end(),
        [](const auto& portData)
        {
            return portData.outputName == kOpenDoorPortName;
        });

    return portIter != portDescriptions.end();
}

} // namespace

namespace nx::vms::common {

QnUuid calculateIntercomLayoutId(const QnResourcePtr& intercom)
{
    return calculateIntercomLayoutId(intercom->getId());
}

QnUuid calculateIntercomLayoutId(const QnUuid& intercomId)
{
    return guidFromArbitraryData(intercomId.toString() + "layout");
}

// WARNING: For now it can return false negative result if an intercom is in removing process.
// Because any intercom-specific feature could be already removed from resource.
bool isIntercom(const QnResourcePtr& resource)
{
    if (!resource)
        return false;

    return checkIntercomCallPortExists(resource);
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

    return checkIntercomCallPortExists(resource);
}

bool isIntercomLayout(const QnResourcePtr& layoutResource)
{
    if (!layoutResource || !layoutResource->hasFlags(Qn::layout))
        return false;

    const auto parentId = layoutResource->getParentId();
    if (parentId.isNull())
        return false;

    // The function could be called while intercom is in removing process.
    // In this case all intercom-specific featires could be already removed from intercom resource.
    // So, it is the only robust way to identify intercom layout for now.
    const auto parent = layoutResource->getParentResource();
    return parent && parent->hasFlags(Qn::video);
}

} // namespace nx::vms::common