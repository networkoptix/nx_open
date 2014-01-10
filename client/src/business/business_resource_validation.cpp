#include "business_resource_validation.h"

#include <core/resource/resource.h>
#include <core/resource/resource_name.h>
#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>

#include <utils/common/email.h>

namespace {

    class QnBusinessResourceValidationStrings {
        Q_DECLARE_TR_FUNCTIONS(QnBusinessResourceValidationStrings)
    public:
        static QString anyCamera() { return tr("<Any Camera>"); }
        static QString selectCamera() { return tr("Select at least one camera"); }
        static QString multipleCameras(int total) { return tr("%n Camera(s)", "", total); }
        static QString subsetCameras(int count, int total) { return tr("%1 of %n cameras", "...for", total).arg(count); }
    };

    template <typename CheckingPolicy>
    int invalidResourcesCount(const QnResourceList &resources) {
        typedef typename CheckingPolicy::resource_type ResourceType;

        QnSharedResourcePointerList<ResourceType> filtered = resources.filtered<ResourceType>();
        int invalid = 0;
        foreach (const QnSharedResourcePointer<ResourceType> &resource, filtered)
            if (!CheckingPolicy::isResourceValid(resource))
                invalid++;
        return invalid;
    }

    template <typename CheckingPolicy>
    QString genericCameraText(const QnVirtualCameraResourceList &cameras, const bool detailed, const QString &baseText) {
        if (cameras.isEmpty())
            return CheckingPolicy::emptyListIsValid()
                    ? QnBusinessResourceValidationStrings::anyCamera()
                    : QnBusinessResourceValidationStrings::selectCamera();

        int invalid = invalidResourcesCount<CheckingPolicy>(cameras);
        if (detailed && invalid > 0)
            return baseText.arg(
                        (cameras.size() == 1)
                         ? getShortResourceName(cameras.first())
                         : QnBusinessResourceValidationStrings::subsetCameras(invalid, cameras.size())
                           );
        if (cameras.size() == 1)
            return getShortResourceName(cameras.first());
        return QnBusinessResourceValidationStrings::multipleCameras(cameras.size());

    }

}

bool QnCameraInputPolicy::isResourceValid(const QnVirtualCameraResourcePtr &camera) {
    return (camera->getCameraCapabilities() & Qn::RelayInputCapability);
}

QString QnCameraInputPolicy::getText(const QnResourceList &resources, const bool detailed) {
    QnVirtualCameraResourceList cameras = resources.filtered<QnVirtualCameraResource>();
    return genericCameraText<QnCameraInputPolicy>(cameras, detailed, tr("%1 have not input ports", "", cameras.size()));
}

bool QnCameraOutputPolicy::isResourceValid(const QnVirtualCameraResourcePtr &camera) {
    return camera->getCameraCapabilities() & Qn::RelayOutputCapability;
}

QString QnCameraOutputPolicy::getText(const QnResourceList &resources, const bool detailed) {
    QnVirtualCameraResourceList cameras = resources.filtered<QnVirtualCameraResource>();
    return genericCameraText<QnCameraOutputPolicy>(cameras, detailed, tr("%1 have not output relays", "", cameras.size()));
}

bool QnCameraMotionPolicy::isResourceValid(const QnVirtualCameraResourcePtr &camera) {
    return !camera->isScheduleDisabled()
            && camera->getMotionType() != Qn::MT_NoMotion
            && camera->supportedMotionType() != Qn::MT_NoMotion;
}

QString QnCameraMotionPolicy::getText(const QnResourceList &resources, const bool detailed) {
    QnVirtualCameraResourceList cameras = resources.filtered<QnVirtualCameraResource>();
    return genericCameraText<QnCameraMotionPolicy>(cameras, detailed, tr("Recording or motion detection is disabled for %1", "", cameras.size()));
}

bool QnCameraRecordingPolicy::isResourceValid(const QnVirtualCameraResourcePtr &camera) {
    return !camera->isScheduleDisabled();
}

QString QnCameraRecordingPolicy::getText(const QnResourceList &resources, const bool detailed) {
    QnVirtualCameraResourceList cameras = resources.filtered<QnVirtualCameraResource>();
    return genericCameraText<QnCameraRecordingPolicy>(cameras, detailed, tr("Recording is disabled for %1", "", cameras.size()));
}

bool QnUserEmailPolicy::isResourceValid(const QnUserResourcePtr &resource) {
    return QnEmail::isValid(resource->getEmail());
}

QString QnUserEmailPolicy::getText(const QnResourceList &resources, const bool detailed, const QStringList &additional) {
    QStringList receivers;
    QnUserResourceList users =  resources.filtered<QnUserResource>();
    foreach (const QnUserResourcePtr &user, users) {
        QString userMail = user->getEmail();
        if (userMail.isEmpty())
            return tr("User '%1' has empty E-Mail").arg(user->getName());
        if (!QnEmail::isValid(userMail))
            return tr("User '%1' has invalid E-Mail address: %2").arg(user->getName()).arg(userMail);
        receivers << QString(QLatin1String("%1 <%2>")).arg(user->getName()).arg(userMail);
    }


    foreach(const QString &email, additional) {
        QString trimmed = email.trimmed();
        if (trimmed.isEmpty())
            continue;
        if (!QnEmail::isValid(trimmed))
            return tr("Invalid email address: %1").arg(trimmed);
        receivers << trimmed;
    }

    if (receivers.isEmpty())
        return tr("Select at least one user");

    if (detailed)
        return tr("Send mail to %1").arg(receivers.join(QLatin1String("; ")));
    if (additional.size() > 0)
        return tr("%1 users, %2 additional").arg(users.size()).arg(additional.size());
    return tr("%1 users").arg(users.size());
}
