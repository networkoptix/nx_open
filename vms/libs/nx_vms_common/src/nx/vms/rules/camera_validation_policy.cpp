// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_validation_policy.h"

#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/layout_resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource_management/resource_pool.h>

using namespace nx::vms::api;

namespace {

QString braced(const QString& source)
{
    return NX_FMT("<%1>", source);
}

QString getShortResourceName(const QnResourcePtr& resource)
{
    return QnResourceDisplayInfo(resource).toString(Qn::RI_NameOnly);
}

class QnBusinessResourceValidationStrings
{
    Q_DECLARE_TR_FUNCTIONS(QnBusinessResourceValidationStrings)
public:
    static QString subsetCameras(
        nx::vms::common::SystemContext* context,
        int count,
        const QnVirtualCameraResourceList& total)
    {
        const auto totalCount = total.size();
        return QnDeviceDependentStrings::getNameFromSet(
            context->resourcePool(),
            QnCameraDeviceStringSet(
                tr("%1 of %n devices", "", totalCount),
                tr("%1 of %n cameras", "", totalCount),
                tr("%1 of %n I/O modules", "", totalCount)
            ), total
        ).arg(count);
    }

    static QString anyCamera(nx::vms::common::SystemContext* context)
    {
        return braced(QnDeviceDependentStrings::getDefaultNameFromSet(
            context->resourcePool(),
            tr("Any Device"),
            tr("Any Camera")));
    }

    static QString selectCamera(nx::vms::common::SystemContext* context)
    {
        return QnDeviceDependentStrings::getDefaultNameFromSet(
            context->resourcePool(),
            tr("Select at least one device"),
            tr("Select at least one camera"));
    }
};

template <typename CheckingPolicy>
QString genericCameraText(
    nx::vms::common::SystemContext* context,
    const QnVirtualCameraResourceList& cameras,
    const bool detailed,
    const QString& baseText,
    int invalid)
{
    if (cameras.isEmpty())
    {
        return CheckingPolicy::emptyListIsValid()
            ? QnBusinessResourceValidationStrings::anyCamera(context)
            : QnBusinessResourceValidationStrings::selectCamera(context);
    }

    if (detailed && invalid > 0)
    {
        return baseText.arg((cameras.size() == 1)
            ? getShortResourceName(cameras.first())
            : QnBusinessResourceValidationStrings::subsetCameras(context, invalid, cameras));
    }

    if (cameras.size() == 1)
        return getShortResourceName(cameras.first());

    return QnDeviceDependentStrings::getNumericName(context->resourcePool(), cameras);
}

template <typename CheckingPolicy>
int invalidResourcesCount(nx::vms::common::SystemContext* context, const QnResourceList &resources)
{
    typedef typename CheckingPolicy::resource_type ResourceType;

    QnSharedResourcePointerList<ResourceType> filtered = resources.filtered<ResourceType>();
    int invalid = 0;
    for(const auto& resource: filtered)
    {
        if (!CheckingPolicy::isResourceValid(context, resource))
            invalid++;
    }

    return invalid;
}

} // namespace

QString QnAllowAnyCameraPolicy::getText(
    nx::vms::common::SystemContext* context,
    const QnResourceList& resources,
    const bool detailed)
{
    QnVirtualCameraResourceList cameras = resources.filtered<QnVirtualCameraResource>();
    return genericCameraText<QnAllowAnyCameraPolicy>(
        context, cameras, detailed, "", /* invalid */ 0);
}

QString QnRequireCameraPolicy::getText(
    nx::vms::common::SystemContext* context,
    const QnResourceList& resources,
    const bool detailed)
{
    QnVirtualCameraResourceList cameras = resources.filtered<QnVirtualCameraResource>();
    return genericCameraText<QnRequireCameraPolicy>(
        context, cameras, detailed, "", /* invalid */ 0);
}

bool QnCameraInputPolicy::isResourceValid(
    nx::vms::common::SystemContext* /*context*/, const QnVirtualCameraResourcePtr &camera)
{
    return camera->hasCameraCapabilities(DeviceCapability::inputPort);
}

QString QnCameraInputPolicy::getText(
    nx::vms::common::SystemContext* context,
    const QnResourceList& resources,
    const bool detailed)
{
    QnVirtualCameraResourceList cameras = resources.filtered<QnVirtualCameraResource>();
    int invalid = invalidResourcesCount<QnCameraInputPolicy>(context, cameras);
    return genericCameraText<QnCameraInputPolicy>(
        context, cameras, detailed, tr("%1 have no input ports", "", invalid), invalid);
}

bool QnCameraOutputPolicy::isResourceValid(
    nx::vms::common::SystemContext* /*context*/, const QnVirtualCameraResourcePtr &camera)
{
    return camera->hasCameraCapabilities(DeviceCapability::outputPort);
}

QString QnCameraOutputPolicy::getText(
    nx::vms::common::SystemContext* context,
    const QnResourceList& resources,
    const bool detailed)
{
    QnVirtualCameraResourceList cameras = resources.filtered<QnVirtualCameraResource>();
    int invalid = invalidResourcesCount<QnCameraOutputPolicy>(context, cameras);
    return genericCameraText<QnCameraOutputPolicy>(
        context, cameras, detailed, tr("%1 have no output relays", "", invalid), invalid);
}

bool QnExecPtzPresetPolicy::isResourceValid(
    nx::vms::common::SystemContext* /*context*/, const QnVirtualCameraResourcePtr& camera)
{
    return camera->hasAnyOfPtzCapabilities(Ptz::PresetsPtzCapability)
        || camera->getDewarpingParams().enabled;
}

QString QnExecPtzPresetPolicy::getText(
    nx::vms::common::SystemContext* context,
    const QnResourceList& resources,
    const bool /*detailed*/)
{
    QnVirtualCameraResourceList cameras = resources.filtered<QnVirtualCameraResource>();
    if (cameras.size() != 1)
        return tr("Select exactly one camera");

    QnVirtualCameraResourcePtr camera = cameras.first();
    if (!isResourceValid(context, camera))
        return tr("%1 has no PTZ presets").arg(getShortResourceName(camera));

    return getShortResourceName(camera);
}

bool QnCameraMotionPolicy::isResourceValid(
    nx::vms::common::SystemContext* /*context*/, const QnVirtualCameraResourcePtr& camera)
{
    return camera->isScheduleEnabled() && camera->isMotionDetectionSupported();
}

QString QnCameraMotionPolicy::getText(
    nx::vms::common::SystemContext* context,
    const QnResourceList& resources,
    const bool detailed)
{
    QnVirtualCameraResourceList cameras = resources.filtered<QnVirtualCameraResource>();
    int invalid = invalidResourcesCount<QnCameraMotionPolicy>(context, cameras);
    return genericCameraText<QnCameraMotionPolicy>(
        context, cameras, detailed, tr("Recording or motion detection is disabled for %1"), invalid);
}

bool QnCameraAudioTransmitPolicy::isResourceValid(
    nx::vms::common::SystemContext* /*context*/, const QnVirtualCameraResourcePtr& camera)
{
    return camera->hasTwoWayAudio();
}

QString QnCameraAudioTransmitPolicy::getText(
    nx::vms::common::SystemContext* context,
    const QnResourceList& resources,
    const bool detailed)
{
    QnVirtualCameraResourceList cameras = resources.filtered<QnVirtualCameraResource>();
    int invalid = invalidResourcesCount<QnCameraAudioTransmitPolicy>(context, cameras);
    if (cameras.isEmpty())
    {
        return QnDeviceDependentStrings::getDefaultNameFromSet(
            context->resourcePool(),
            tr("Select device"),
            tr("Select camera"));
    }

    return genericCameraText<QnCameraAudioTransmitPolicy>(
        context, cameras, detailed, tr("%1 does not support two-way audio", "", invalid), invalid);
}

bool QnCameraRecordingPolicy::isResourceValid(
    nx::vms::common::SystemContext* /*context*/, const QnVirtualCameraResourcePtr& camera)
{
    return camera->isScheduleEnabled();
}

QString QnCameraRecordingPolicy::getText(
    nx::vms::common::SystemContext* context,
    const QnResourceList& resources,
    const bool detailed)
{
    QnVirtualCameraResourceList cameras = resources.filtered<QnVirtualCameraResource>();
    int invalid = invalidResourcesCount<QnCameraRecordingPolicy>(context, cameras);
    return genericCameraText<QnCameraRecordingPolicy>(
        context, cameras, detailed, tr("Recording is disabled for %1"), invalid);
}

bool QnCameraAnalyticsPolicy::isResourceValid(
    nx::vms::common::SystemContext* /*context*/, const QnVirtualCameraResourcePtr& camera)
{
    return !camera->compatibleAnalyticsEngines().isEmpty();
}

QString QnCameraAnalyticsPolicy::getText(
    nx::vms::common::SystemContext* context,
    const QnResourceList& resources,
    const bool detailed)
{
    const auto cameras = resources.filtered<QnVirtualCameraResource>();

    int invalid = invalidResourcesCount<QnCameraAnalyticsPolicy>(context, cameras);
    return genericCameraText<QnCameraAnalyticsPolicy>(
        context, cameras, detailed, tr("Analytics is not available for %1"), invalid);
}

void QnFullscreenCameraPolicy::setLayouts(QnLayoutResourceList layouts)
{
    m_layouts = std::move(layouts);
}

bool QnFullscreenCameraPolicy::isResourceValid(
    nx::vms::common::SystemContext* context, const QnVirtualCameraResourcePtr& camera) const
{
    for (const auto& layout: m_layouts)
    {
        const auto layoutItem = layout->getItems();
        bool isCameraOnLayout = std::any_of(
            layoutItem.cbegin(),
            layoutItem.cend(),
            [&camera, context](const nx::vms::common::LayoutItemData& item)
            {
                return context->resourcePool()->getResourceByDescriptor(item.resource) == camera;
            });

        if (!isCameraOnLayout)
            return false;
    }

    return true;
}

QString QnFullscreenCameraPolicy::getText(
    nx::vms::common::SystemContext* context,
    const QnResourceList& resources,
    const bool /*detailed*/) const
{
    const auto cameras = resources.filtered<QnVirtualCameraResource>();
    if (cameras.empty())
        return QnBusinessResourceValidationStrings::selectCamera(context);

    if (cameras.size() != 1)
        return tr("Select exactly one camera");

    if (!isResourceValid(context, cameras.first()))
    {
        return m_layouts.size() == 1
            ? tr("This camera is not currently on the selected layout")
            : tr("This camera is not currently on some of the selected layouts");
    }

    return getShortResourceName(cameras.first());
}
