#include "business_resource_validation.h"

#include <core/resource/resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_access_manager.h>

#include <utils/email/email.h>

namespace
{

    QString getShortResourceName(const QnResourcePtr& resource)
    {
        return QnResourceDisplayInfo(resource).toString(Qn::RI_NameOnly);
    }

    class QnBusinessResourceValidationStrings {
        Q_DECLARE_TR_FUNCTIONS(QnBusinessResourceValidationStrings)
    public:
        static QString subsetCameras(int count, const QnVirtualCameraResourceList &total) {
            const auto totalCount = total.size();
            return QnDeviceDependentStrings::getNameFromSet(
                QnCameraDeviceStringSet(
                    tr("%1 of %n devices", "", totalCount),
                    tr("%1 of %n cameras", "", totalCount),
                    tr("%1 of %n I/O modules", "", totalCount)
                ), total
            ).arg(count);
        }

        static QString anyCamera() {
            return QnDeviceDependentStrings::getDefaultNameFromSet(
                tr("<Any Device>"),
                tr("<Any Camera>")
                );
        }

        static QString selectCamera() {
            return QnDeviceDependentStrings::getDefaultNameFromSet(
                tr("Select at least one device"),
                tr("Select at least one camera")
                );
        }
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
                         : QnBusinessResourceValidationStrings::subsetCameras(invalid, cameras)
                           );
        if (cameras.size() == 1)
            return getShortResourceName(cameras.first());
        return QnDeviceDependentStrings::getNumericName(cameras);
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
    int invalid = invalidResourcesCount<QnCameraOutputPolicy>(cameras);
    return genericCameraText<QnCameraOutputPolicy>(cameras, detailed, tr("%1 have no output relays", "", invalid), invalid);
}

bool QnExecPtzPresetPolicy::isResourceValid(const QnVirtualCameraResourcePtr &camera) {
    return  camera->hasAnyOfPtzCapabilities(Qn::PresetsPtzCapability) || //|| camera->hasPtzCapabilities(Qn::VirtualPtzCapability);
            camera->getDewarpingParams().enabled;
}

QString QnExecPtzPresetPolicy::getText(const QnResourceList &resources, const bool detailed) {
    Q_UNUSED(detailed);
    QnVirtualCameraResourceList cameras = resources.filtered<QnVirtualCameraResource>();
    if (cameras.size() != 1)
        return tr("Select exactly one camera");

    QnVirtualCameraResourcePtr camera = cameras.first();
    if (!isResourceValid(camera))
        return tr("%1 has no ptz presets").arg(getShortResourceName(camera));

    return getShortResourceName(camera);
}

bool QnCameraMotionPolicy::isResourceValid(const QnVirtualCameraResourcePtr &camera) {
    return !camera->isScheduleDisabled() && camera->hasMotion();
}

QString QnCameraMotionPolicy::getText(const QnResourceList &resources, const bool detailed) {
    QnVirtualCameraResourceList cameras = resources.filtered<QnVirtualCameraResource>();
    int invalid = invalidResourcesCount<QnCameraMotionPolicy>(cameras);
    return genericCameraText<QnCameraMotionPolicy>(cameras, detailed, tr("Recording or motion detection is disabled for %1", "", invalid), invalid);
}

bool QnCameraAudioTransmitPolicy::isResourceValid(const QnVirtualCameraResourcePtr &camera) {
    return camera->hasCameraCapabilities(Qn::AudioTransmitCapability);
}

QString QnCameraAudioTransmitPolicy::getText(const QnResourceList &resources, const bool detailed) {
    QnVirtualCameraResourceList cameras = resources.filtered<QnVirtualCameraResource>();
    int invalid = invalidResourcesCount<QnCameraAudioTransmitPolicy>(cameras);
    if (cameras.isEmpty())
        return tr("Select device");
    else
        return genericCameraText<QnCameraAudioTransmitPolicy>(cameras, detailed, tr("%1 doesn't support two-way audio", "", invalid), invalid);
}

bool QnCameraRecordingPolicy::isResourceValid(const QnVirtualCameraResourcePtr &camera) {
    return !camera->isScheduleDisabled();
}

QString QnCameraRecordingPolicy::getText(const QnResourceList &resources, const bool detailed) {
    QnVirtualCameraResourceList cameras = resources.filtered<QnVirtualCameraResource>();
    int invalid = invalidResourcesCount<QnCameraRecordingPolicy>(cameras);
    return genericCameraText<QnCameraRecordingPolicy>(cameras, detailed, tr("Recording is disabled for %1", "", invalid), invalid);
}

QnSendEmailActionDelegate::QnSendEmailActionDelegate(QWidget* parent):
    QnResourceSelectionDialogDelegate(parent),
    m_warningLabel(nullptr)
{
}

void QnSendEmailActionDelegate::init(QWidget* parent)
{
    m_warningLabel = new QLabel(parent);
    setWarningStyle(m_warningLabel);
    parent->layout()->addWidget(m_warningLabel);
}

bool QnSendEmailActionDelegate::validate(const QSet<QnUuid>& selected)
{
    if (!m_warningLabel)
        return true;

    bool valid = isValidList(selected);
    m_warningLabel->setVisible(!valid);
    if (!valid)
        m_warningLabel->setText(getText(selected));
    return true;
}

bool QnSendEmailActionDelegate::isValid(const QnUuid& resourceId) const
{
    if (auto user = qnResPool->getResourceById<QnUserResource>(resourceId))
        return isValidUser(user);
    return true;
}

bool QnSendEmailActionDelegate::isValidList(const QSet<QnUuid>& ids, const QString& additional)
{
    bool any = false;

    auto roles = qnResourceAccessManager->userGroups(ids);
    if (!roles.empty())
        any = true;

    auto users = qnResPool->getResources<QnUserResource>(ids);
    for (auto user : users)
    {
        QString email = user->getEmail();
        if (email.isEmpty() || !QnEmailAddress::isValid(email))
            return false;
        any = true;
    }

    for (const auto& email : parseAdditional(additional))
    {
        if (!QnEmailAddress::isValid(email))
            return false;
        any = true;
    }

    return any;
}

QString QnSendEmailActionDelegate::getText(const QSet<QnUuid>& ids, const bool detailed,
    const QString& additionalList)
{
    auto roles = qnResourceAccessManager->userGroups(ids);
    auto users = qnResPool->getResources<QnUserResource>(ids);
    auto additional = parseAdditional(additionalList);

    if (users.isEmpty() && roles.empty() && additional.isEmpty())
        return tr("Select at least one user");

    QStringList receivers;
    int invalid = 0;
    for (const auto& user: users)
    {
        QString userMail = user->getEmail();
        if (isValidUser(user))
            receivers << lit("%1 <%2>").arg(user->getName()).arg(userMail);
        else
            invalid++;
    }

    for (const auto& role: roles)
        receivers << role.name;

    if (detailed && invalid > 0)
    {
        if (users.size() == 1)
            return tr("User %1 has invalid email address").arg(users.first()->getName());
        return tr("%n of %1 users have invalid email address", "", invalid).arg(users.size());
    }

    invalid = 0;
    for (const QString &email: additional)
    {
        if (QnEmailAddress::isValid(email))
            receivers << email;
        else
            invalid++;
    }

    //
    if (detailed && invalid > 0)
        return (additional.size() == 1)
        ? tr("Invalid email address %1").arg(additional.first())
        : tr("%n of %1 additional email addresses are invalid", "", invalid).arg(additional.size());

    if (detailed)
        return tr("Send email to %1").arg(receivers.join(QLatin1String("; ")));


    QStringList recipients;
    if (!users.empty())
        recipients << tr("%n Users", "", users.size());
    if (!roles.empty())
        recipients << tr("%n Roles", "", roles.size());
    if (!additional.empty())
        recipients << tr("%n additional", "", additional.size());

    NX_ASSERT(!recipients.empty());
    return recipients.join(lit(", "));
}

QStringList QnSendEmailActionDelegate::parseAdditional(const QString& additional)
{
    QStringList result;
    for (auto email : additional.split(L';', QString::SkipEmptyParts))
    {
        if (email.trimmed().isEmpty())
            continue;
        result << email;
    }
    result.removeDuplicates();
    return result;
}

bool QnSendEmailActionDelegate::isValidUser(const QnUserResourcePtr& user)
{
    return QnEmailAddress::isValid(user->getEmail());
}
