#include "business_resource_validation.h"

#include <core/resource/resource.h>
#include <core/resource/resource_name.h>
#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>

#include <utils/common/email.h>

template <typename CheckingPolicy>
int invalidResourcesCount(const QnResourceList &resources) {
    QnSharedResourcePointerList<typename CheckingPolicy::resource_type > filtered = resources.filtered<typename CheckingPolicy::resource_type >();
    int invalid = 0;
    foreach (const QnSharedResourcePointer<typename CheckingPolicy::resource_type > &resource, filtered)
        if (!CheckingPolicy::isResourceValid(resource))
            invalid++;
    return invalid;
}


bool QnCameraInputAllowedPolicy::isResourceValid(const QnVirtualCameraResourcePtr &camera) {
    return (camera->getCameraCapabilities() & Qn::RelayInputCapability);
}

QString QnCameraInputAllowedPolicy::getText(const QnResourceList &resources, const bool detailed) {
return QString(); //  return tr("%1 of %2 selected cameras have no input ports.").arg(invalid).arg(total);

}

bool QnCameraOutputAllowedPolicy::isResourceValid(const QnVirtualCameraResourcePtr &camera) {
    return camera->getCameraCapabilities() & Qn::RelayOutputCapability;
}

QString QnCameraOutputAllowedPolicy::getText(const QnResourceList &resources, const bool detailed) {
    QnVirtualCameraResourceList cameras = resources.filtered<QnVirtualCameraResource>();
    if (cameras.isEmpty())
        return tr("Select at least one camera");

    int invalid = invalidResourcesCount<QnCameraOutputAllowedPolicy>(cameras);
    if (detailed && invalid > 0)
        return tr("%1 have not output relays", "", cameras.size())
                .arg((cameras.size() == 1)
                     ? getShortResourceName(cameras.first())
                     : tr("%1 of %n cameras", "...for", cameras.size()).arg(invalid));
    if (cameras.size() == 1)
        return getShortResourceName(cameras.first());
    return tr("%n Camera(s)", "", cameras.size());
}

bool QnCameraMotionAllowedPolicy::isResourceValid(const QnVirtualCameraResourcePtr &camera) {
    return !camera->isScheduleDisabled()
            && camera->getMotionType() != Qn::MT_NoMotion
            && camera->supportedMotionType() != Qn::MT_NoMotion;
}

QString QnCameraMotionAllowedPolicy::getText(const QnResourceList &resources, const bool detailed) {
    QnVirtualCameraResourceList cameras = resources.filtered<QnVirtualCameraResource>();
    if (cameras.isEmpty())
        return tr("<Any Camera>");

    int invalid = invalidResourcesCount<QnCameraMotionAllowedPolicy>(cameras);
    if (detailed && invalid > 0)
        return tr("Recording or motion detection is disabled for %1")
                .arg((cameras.size() == 1)
                     ? getShortResourceName(cameras.first())
                     : tr("%1 of %n cameras", "...for", cameras.size()).arg(invalid));
    if (cameras.size() == 1)
        return getShortResourceName(cameras.first());
    return tr("%n Camera(s)", "", cameras.size());
}


bool QnCameraRecordingAllowedPolicy::isResourceValid(const QnVirtualCameraResourcePtr &camera) {
    return !camera->isScheduleDisabled();
}

QString QnCameraRecordingAllowedPolicy::getText(const QnResourceList &resources, const bool detailed) {
    QnVirtualCameraResourceList cameras = resources.filtered<QnVirtualCameraResource>();
    if (cameras.isEmpty())
        return tr("Select at least one camera");

    int invalid = invalidResourcesCount<QnCameraRecordingAllowedPolicy>(cameras);
    if (detailed && invalid > 0)
        return tr("Recording is disabled for %1")
                .arg((cameras.size() == 1)
                     ? getShortResourceName(cameras.first())
                     : tr("%1 of %2 cameras").arg(invalid).arg(cameras.size()));
    if (cameras.size() == 1)
        return getShortResourceName(cameras.first());
    return tr("%n Camera(s)", "", cameras.size());
}

bool QnUserEmailAllowedPolicy::isResourceValid(const QnUserResourcePtr &resource) {
    return QnEmail::isValid(resource->getEmail());
}

QString QnUserEmailAllowedPolicy::getText(const QnResourceList &resources, const bool detailed) {
return QString(); //    return tr("%1 of %2 selected users have invalid email.").arg(invalid).arg(total);
}
