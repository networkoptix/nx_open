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
        static QString subsetCameras(int count, int total) { return tr("%n of %1 cameras", "", count).arg(total); } 
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
    QString genericCameraText(const QnVirtualCameraResourceList &cameras, const bool detailed, const QString &baseText, int invalid) {
        if (cameras.isEmpty())
            return CheckingPolicy::emptyListIsValid()
                    ? QnBusinessResourceValidationStrings::anyCamera()
                    : QnBusinessResourceValidationStrings::selectCamera();

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
    int invalid = invalidResourcesCount<QnCameraInputPolicy>(cameras);
    return genericCameraText<QnCameraInputPolicy>(cameras, detailed, tr("%1 have no input ports", "", invalid), invalid);
}

bool QnCameraOutputPolicy::isResourceValid(const QnVirtualCameraResourcePtr &camera) {
    return camera->getCameraCapabilities() & Qn::RelayOutputCapability;
}

QString QnCameraOutputPolicy::getText(const QnResourceList &resources, const bool detailed) {
    QnVirtualCameraResourceList cameras = resources.filtered<QnVirtualCameraResource>();
    int invalid = invalidResourcesCount<QnCameraInputPolicy>(cameras);
    return genericCameraText<QnCameraOutputPolicy>(cameras, detailed, tr("%1 have no output relays", "", invalid), invalid);
}

bool QnCameraMotionPolicy::isResourceValid(const QnVirtualCameraResourcePtr &camera) {
    return !camera->isScheduleDisabled()
            && camera->getMotionType() != Qn::MT_NoMotion
            && camera->supportedMotionType() != Qn::MT_NoMotion;
}

QString QnCameraMotionPolicy::getText(const QnResourceList &resources, const bool detailed) {
    QnVirtualCameraResourceList cameras = resources.filtered<QnVirtualCameraResource>();
    int invalid = invalidResourcesCount<QnCameraInputPolicy>(cameras);
    return genericCameraText<QnCameraMotionPolicy>(cameras, detailed, tr("Recording or motion detection is disabled for %1", "", invalid), invalid);
}

bool QnCameraRecordingPolicy::isResourceValid(const QnVirtualCameraResourcePtr &camera) {
    return !camera->isScheduleDisabled();
}

QString QnCameraRecordingPolicy::getText(const QnResourceList &resources, const bool detailed) {
    QnVirtualCameraResourceList cameras = resources.filtered<QnVirtualCameraResource>();
    int invalid = invalidResourcesCount<QnCameraInputPolicy>(cameras);
    return genericCameraText<QnCameraRecordingPolicy>(cameras, detailed, tr("Recording is disabled for %1", "", invalid), invalid);
}

bool QnUserEmailPolicy::isResourceValid(const QnUserResourcePtr &resource) {
    return QnEmail::isValid(resource->getEmail());
}

QString QnUserEmailPolicy::getText(const QnResourceList &resources, const bool detailed, const QStringList &additional) {

    QnUserResourceList users = resources.filtered<QnUserResource>();
    if (users.isEmpty() && additional.isEmpty())
        return tr("Select at least one user");

    QStringList receivers;
    int invalid = 0;
    foreach (const QnUserResourcePtr &user, users) {
        QString userMail = user->getEmail();
        if (isResourceValid(user))
            receivers << lit("%1 <%2>").arg(user->getName()).arg(userMail);
        else
            invalid++;
    }

    // TODO: #GDM #TR string composition won't work well in russian.
    // See for yourself:
    //
    // %1 imeyut nevalidnyi e-mail adres
    // Polzovatel GDM
    // =>
    // Polzovatel GDM imeyut nevalidnyi e-mail adres
    // 
    // Split these into two full strings.
    if (detailed && invalid > 0)
        return tr("%1 have invalid E-Mail address", "", invalid).arg(
                    (users.size() == 1)
                     ? tr("User %1").arg(users.first()->getName())
                     : tr("%n of %1 users", "", invalid).arg(users.size())
                       );

    invalid = 0;
    foreach(const QString &email, additional) {
        if (QnEmail::isValid(email))
            receivers << email;
        else
            invalid++;
    }

    //
    if (detailed && invalid > 0)
        return (additional.size() == 1)
                ? tr("Invalid E-Mail address %1").arg(additional.first())
                : tr("%n of %1 additional E-Mail addresses are invalid", "", invalid).arg(additional.size());

    if (detailed)
        return tr("Send mail to %1").arg(receivers.join(QLatin1String("; ")));

    QString result = tr("%n User(s)", "", users.size());
    if (additional.size() > 0)
        result = tr("%1, %n additional", "", additional.size()).arg(result);
    return result;
}
