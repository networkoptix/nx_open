#include "business_resource_validation.h"

#include <QtWidgets/QLayout>

#include <nx/vms/event/events/abstract_event.h>
#include <nx/vms/event/action_parameters.h>
#include <nx/vms/event/strings_helper.h>

#include <common/common_module.h>
#include <client_core/client_core_module.h>

#include <core/resource/resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_subjects_cache.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource_access/resource_access_manager.h>

#include <nx/api/analytics/driver_manifest.h>
#include <nx/api/analytics/supported_events.h>

#include <utils/email/email.h>

namespace {

static const QString kForceHtml = lit("<html>%1</html>");

QString braced(const QString& source)
{
    return L'<' + source + L'>';
}

QString getShortResourceName(const QnResourcePtr& resource)
{
    return QnResourceDisplayInfo(resource).toString(Qn::RI_NameOnly);
}

int countEnabledUsers(const QnUserResourceList& users)
{
    return std::count_if(users.cbegin(), users.cend(),
        [](const QnUserResourcePtr& user) -> bool
        {
            return user && user->isEnabled();
        });
}

class QnBusinessResourceValidationStrings
{
    Q_DECLARE_TR_FUNCTIONS(QnBusinessResourceValidationStrings)
public:
    static QString subsetCameras(int count, const QnVirtualCameraResourceList &total)
    {
        const auto totalCount = total.size();
        return QnDeviceDependentStrings::getNameFromSet(
            qnClientCoreModule->commonModule()->resourcePool(),
            QnCameraDeviceStringSet(
                tr("%1 of %n devices", "", totalCount),
                tr("%1 of %n cameras", "", totalCount),
                tr("%1 of %n I/O modules", "", totalCount)
            ), total
        ).arg(count);
    }

    static QString anyCamera()
    {
        return braced(QnDeviceDependentStrings::getDefaultNameFromSet(
            qnClientCoreModule->commonModule()->resourcePool(),
            tr("Any Device"),
            tr("Any Camera")
        ));
    }

    static QString selectCamera()
    {
        return QnDeviceDependentStrings::getDefaultNameFromSet(
            qnClientCoreModule->commonModule()->resourcePool(),
            tr("Select at least one device"),
            tr("Select at least one camera")
        );
    }
};

template <typename CheckingPolicy>
int invalidResourcesCount(const QnResourceList &resources)
{
    typedef typename CheckingPolicy::resource_type ResourceType;

    QnSharedResourcePointerList<ResourceType> filtered = resources.filtered<ResourceType>();
    int invalid = 0;
    foreach(const QnSharedResourcePointer<ResourceType> &resource, filtered)
        if (!CheckingPolicy::isResourceValid(resource))
            invalid++;
    return invalid;
}

template <typename CheckingPolicy>
QString genericCameraText(const QnVirtualCameraResourceList &cameras, const bool detailed, const QString &baseText, int invalid)
{
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
    return QnDeviceDependentStrings::getNumericName(
        qnClientCoreModule->commonModule()->resourcePool(),
        cameras);
}

}

//-------------------------------------------------------------------------------------------------
// QnCameraInputPolicy
//-------------------------------------------------------------------------------------------------

bool QnCameraInputPolicy::isResourceValid(const QnVirtualCameraResourcePtr &camera)
{
    return (camera->getCameraCapabilities() & Qn::RelayInputCapability);
}

QString QnCameraInputPolicy::getText(const QnResourceList &resources, const bool detailed)
{
    QnVirtualCameraResourceList cameras = resources.filtered<QnVirtualCameraResource>();
    int invalid = invalidResourcesCount<QnCameraInputPolicy>(cameras);
    return genericCameraText<QnCameraInputPolicy>(cameras, detailed, tr("%1 have no input ports", "", invalid), invalid);
}

//-------------------------------------------------------------------------------------------------
// QnCameraOutputPolicy
//-------------------------------------------------------------------------------------------------

bool QnCameraOutputPolicy::isResourceValid(const QnVirtualCameraResourcePtr &camera)
{
    return camera->getCameraCapabilities() & Qn::RelayOutputCapability;
}

QString QnCameraOutputPolicy::getText(const QnResourceList &resources, const bool detailed)
{
    QnVirtualCameraResourceList cameras = resources.filtered<QnVirtualCameraResource>();
    int invalid = invalidResourcesCount<QnCameraOutputPolicy>(cameras);
    return genericCameraText<QnCameraOutputPolicy>(cameras, detailed, tr("%1 have no output relays", "", invalid), invalid);
}

//-------------------------------------------------------------------------------------------------
// QnCameraMotionPolicy
//-------------------------------------------------------------------------------------------------

bool QnExecPtzPresetPolicy::isResourceValid(const QnVirtualCameraResourcePtr &camera)
{
    return camera->hasAnyOfPtzCapabilities(Ptz::PresetsPtzCapability)
        || camera->getDewarpingParams().enabled;
}

QString QnExecPtzPresetPolicy::getText(const QnResourceList &resources, const bool detailed)
{
    Q_UNUSED(detailed);
    QnVirtualCameraResourceList cameras = resources.filtered<QnVirtualCameraResource>();
    if (cameras.size() != 1)
        return tr("Select exactly one camera");

    QnVirtualCameraResourcePtr camera = cameras.first();
    if (!isResourceValid(camera))
        return tr("%1 has no PTZ presets").arg(getShortResourceName(camera));

    return getShortResourceName(camera);
}

//-------------------------------------------------------------------------------------------------
// QnCameraMotionPolicy
//-------------------------------------------------------------------------------------------------

bool QnCameraMotionPolicy::isResourceValid(const QnVirtualCameraResourcePtr &camera)
{
    return !camera->isScheduleDisabled() && camera->hasMotion();
}

QString QnCameraMotionPolicy::getText(const QnResourceList &resources, const bool detailed)
{
    QnVirtualCameraResourceList cameras = resources.filtered<QnVirtualCameraResource>();
    int invalid = invalidResourcesCount<QnCameraMotionPolicy>(cameras);
    return genericCameraText<QnCameraMotionPolicy>(cameras, detailed,
        tr("Recording or motion detection is disabled for %1"), invalid);
}

//-------------------------------------------------------------------------------------------------
// QnCameraAudioTransmitPolicy
//-------------------------------------------------------------------------------------------------

bool QnCameraAudioTransmitPolicy::isResourceValid(const QnVirtualCameraResourcePtr &camera)
{
    return camera->hasCameraCapabilities(Qn::AudioTransmitCapability);
}

QString QnCameraAudioTransmitPolicy::getText(const QnResourceList &resources, const bool detailed)
{
    QnVirtualCameraResourceList cameras = resources.filtered<QnVirtualCameraResource>();
    int invalid = invalidResourcesCount<QnCameraAudioTransmitPolicy>(cameras);
    if (cameras.isEmpty())
        return QnDeviceDependentStrings::getDefaultNameFromSet(
            qnClientCoreModule->commonModule()->resourcePool(),
            tr("Select device"),
            tr("Select camera"));
    else
        return genericCameraText<QnCameraAudioTransmitPolicy>(cameras, detailed, tr("%1 does not support two-way audio", "", invalid), invalid);
}

//-------------------------------------------------------------------------------------------------
// QnCameraRecordingPolicy
//-------------------------------------------------------------------------------------------------

bool QnCameraRecordingPolicy::isResourceValid(const QnVirtualCameraResourcePtr &camera)
{
    return !camera->isScheduleDisabled();
}

QString QnCameraRecordingPolicy::getText(const QnResourceList &resources, const bool detailed)
{
    QnVirtualCameraResourceList cameras = resources.filtered<QnVirtualCameraResource>();
    int invalid = invalidResourcesCount<QnCameraRecordingPolicy>(cameras);
    return genericCameraText<QnCameraRecordingPolicy>(cameras, detailed,
        tr("Recording is disabled for %1"), invalid);
}

//-------------------------------------------------------------------------------------------------
// QnCameraAnalyticsPolicy
//-------------------------------------------------------------------------------------------------

bool QnCameraAnalyticsPolicy::isResourceValid(const QnVirtualCameraResourcePtr& camera)
{
    return !camera->analyticsSupportedEvents().isEmpty();
}

QString QnCameraAnalyticsPolicy::getText(const QnResourceList& resources, const bool detailed)
{
    const auto cameras = resources.filtered<QnVirtualCameraResource>();

    int invalid = invalidResourcesCount<QnCameraAnalyticsPolicy>(cameras);
    return genericCameraText<QnCameraAnalyticsPolicy>(cameras, detailed,
        tr("Analytics is not available for %1"), invalid);
}

//-------------------------------------------------------------------------------------------------
// QnSendEmailActionDelegate
//-------------------------------------------------------------------------------------------------

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

QString QnSendEmailActionDelegate::validationMessage(const QSet<QnUuid>& selected) const
{
    const bool valid = isValidList(selected, QString()); // TODO: FIXME! Why additional is empty?
    return valid ? QString() : getText(selected, true, QString());
}

bool QnSendEmailActionDelegate::isValid(const QnUuid& resourceId) const
{
    if (resourceId.isNull()) //< custom permissions "Custom" role is not accepted.
        return false;

    if (auto user = resourcePool()->getResourceById<QnUserResource>(resourceId))
        return isValidUser(user);

    /* We can get here either user id or role id. User should be checked additionally, role is
     * always counted as valid (if exists). */
    return !userRolesManager()->userRole(resourceId).isNull()
        || userRolesManager()->predefinedRole(resourceId) != Qn::UserRole::CustomUserRole;
}

bool QnSendEmailActionDelegate::isValidList(const QSet<QnUuid>& ids, const QString& additional)
{
    using boost::algorithm::all_of;

    auto module = qnClientCoreModule->commonModule();

    QnUserResourceList users;
    QList<QnUuid> roles;
    module->userRolesManager()->usersAndRoles(ids, users, roles);

    if (!all_of(users, &isValidUser))
        return false;

    const auto additionalRecipients = parseAdditional(additional);
    if (!all_of(additionalRecipients, nx::email::isValidAddress))
        return false;

    for (const auto& roleId: roles)
    {
        const auto isValidSubject =
            [isValid = &isValidUser](const QnResourceAccessSubject& subject)
            {
                return isValid(subject.user());
            };

        if (!all_of(module->resourceAccessSubjectsCache()->usersInRole(roleId), isValidSubject))
            return false;
    }

    return countEnabledUsers(users) != 0 || !roles.empty() || !additionalRecipients.empty();
}

QString QnSendEmailActionDelegate::getText(const QSet<QnUuid>& ids, const bool detailed,
    const QString& additionalList)
{
    auto module = qnClientCoreModule->commonModule();

    QnUserResourceList users;
    QList<QnUuid> roles;
    module->userRolesManager()->usersAndRoles(ids, users, roles);
    users = users.filtered([](const QnUserResourcePtr& user) { return user->isEnabled(); });

    auto additional = parseAdditional(additionalList);

    if (users.empty() && roles.empty() && additional.isEmpty())
        return nx::vms::event::StringsHelper::needToSelectUserText();

    if (!detailed)
    {
        QStringList recipients;
        if (!users.empty() || !roles.empty())
        {
            recipients << nx::vms::event::StringsHelper(qnClientCoreModule->commonModule())
                .actionSubjects(users, roles, true);
        }

        if (!additional.empty())
            recipients << tr("%n additional", "", additional.size());

        NX_ASSERT(!recipients.empty());
        return recipients.join(lit(", "));
    }

    QStringList receivers;
    QSet<QnUserResourcePtr> invalidUsers;

    for (const auto& user: users)
    {
        QString userMail = user->getEmail();
        if (isValidUser(user))
            receivers << lit("%1 <%2>").arg(user->getName()).arg(userMail);
        else
            invalidUsers << user;
    }

    int total = users.size();

    for (const auto& roleId: roles)
    {
        receivers << module->userRolesManager()->userRoleName(roleId);
        for (const auto& subject: module->resourceAccessSubjectsCache()->usersInRole(roleId))
        {
            const auto& user = subject.user();
            if (!user || !user->isEnabled())
                continue;

            ++total;
            if (!isValidUser(user))
                invalidUsers << user;
        }
    }

    int invalid = invalidUsers.size();
    if (invalid > 0)
    {
        return invalid == 1
            ? tr("User %1 has invalid email address").arg((*invalidUsers.cbegin())->getName())
            : tr("%n of %1 users have invalid email address", "", invalid).arg(total);
    }

    invalid = 0;
    for (const QString& email: additional)
    {
        if (nx::email::isValidAddress(email))
            receivers << email;
        else
            invalid++;
    }

    if (invalid > 0)
    {
        return (additional.size() == 1)
            ? tr("Invalid email address %1").arg(additional.first())
            : tr("%n of %1 additional email addresses are invalid", "", invalid).arg(additional.size());
    }

    return tr("Send email to %1").arg(receivers.join(QLatin1String("; ")));
}

QStringList QnSendEmailActionDelegate::parseAdditional(const QString& additional)
{
    QStringList result;
    for (auto email: additional.split(L';', QString::SkipEmptyParts))
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
    // Disabled users are just not displayed
    if (!user->isEnabled())
        return true;

    return nx::email::isValidAddress(user->getEmail());
}

namespace QnBusiness {

// TODO: #vkutin It's here until full refactoring.
bool actionAllowedForUser(const nx::vms::event::ActionParameters& params,
    const QnUserResourcePtr& user)
{
    if (!user)
        return false;

    if (params.allUsers)
        return true;

    const auto userId = user->getId();
    const auto& subjects = params.additionalResources;

    if (std::find(subjects.cbegin(), subjects.cend(), userId) != subjects.cend())
        return true;

    const auto roleId = QnUserRolesManager::unifiedUserRoleId(user);
    return std::find(subjects.cbegin(), subjects.cend(), roleId) != subjects.cend();
}

bool hasAccessToSource(const nx::vms::event::EventParameters& params,
    const QnUserResourcePtr& user)
{
    if (!user || !user->commonModule())
        return false;

    const auto context = user->commonModule();

    const auto eventType = params.eventType;

    const auto resource = context->resourcePool()->getResourceById(params.eventResourceId);
    const bool hasViewPermission = resource && context->resourceAccessManager()->hasPermission(
        user,
        resource,
        Qn::ViewContentPermission);

    if (nx::vms::event::isSourceCameraRequired(eventType))
    {
        const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
        NX_ASSERT(camera, Q_FUNC_INFO, "Event has occurred without its camera");
        return camera && hasViewPermission;
    }

    if (nx::vms::event::isSourceServerRequired(eventType))
    {
        const auto server = resource.dynamicCast<QnMediaServerResource>();
        NX_ASSERT(server, Q_FUNC_INFO, "Event has occurred without its server");
        /* Only admins should see notifications with servers. */
        return server && hasViewPermission;
    }

    return true;
}

} // namespace QnBusiness

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// QnSubjectValidationPolicy

QnSubjectValidationPolicy::QnSubjectValidationPolicy(bool allowEmptySelection):
    m_allowEmptySelection(allowEmptySelection)
{
}

QnSubjectValidationPolicy::~QnSubjectValidationPolicy()
{
}

void QnSubjectValidationPolicy::analyze(
    bool allUsers,
    const QSet<QnUuid>& subjects,
    QVector<QnUuid>& validRoles,
    QVector<QnUuid>& invalidRoles,
    QVector<QnUuid>& intermediateRoles,
    QVector<QnUserResourcePtr>& validUsers,
    QVector<QnUserResourcePtr>& invalidUsers) const
{
    validRoles.clear();
    invalidRoles.clear();
    intermediateRoles.clear();
    validUsers.clear();
    invalidUsers.clear();

    QnUserResourceList users;
    QList<QnUuid> roleIds;

    if (allUsers)
        users = resourcePool()->getResources<QnUserResource>();
    else
        userRolesManager()->usersAndRoles(subjects, users, roleIds);

    for (const auto& id: roleIds)
    {
        switch (roleValidity(id))
        {
            case QValidator::Acceptable:
                validRoles << id;
                break;

            case QValidator::Intermediate:
                intermediateRoles << id;
                break;

            default: // QValidator::Invalid:
                invalidRoles << id;
                break;
        }
    }

    for (const auto& user: users)
    {
        if (userValidity(user))
            validUsers << user;
        else
            invalidUsers << user;
    }
}

// This function is generally faster than QnSubjectValidationPolicy::analyze.
QValidator::State QnSubjectValidationPolicy::validity(bool allUsers,
    const QSet<QnUuid>& subjects) const
{
    if (!allUsers && subjects.empty() && !m_allowEmptySelection)
        return QValidator::Invalid;

    QnUserResourceList users;
    QList<QnUuid> roleIds;

    if (allUsers)
        users = resourcePool()->getResources<QnUserResource>();
    else
        userRolesManager()->usersAndRoles(subjects, users, roleIds);

    enum StateFlag
    {
        kInvalid = 0x1,
        kValid = 0x2,
        kIntermediate = kInvalid | kValid
    };

    int state = 0;

    for (const auto& id: roleIds)
    {
        switch (roleValidity(id))
        {
            case QValidator::Invalid:
                state |= kInvalid;
                break;

            case QValidator::Intermediate:
                state |= kIntermediate;
                break;

            case QValidator::Acceptable:
                state |= kValid;
                break;

            default:
                NX_ASSERT(false); //< Should never happen.
                break;
        }

        if (state == kIntermediate)
            return QValidator::Intermediate;
    }

    users = users.filtered([](const QnUserResourcePtr& user) { return user->isEnabled(); });
    for (const auto& user: users)
    {
        state |= (userValidity(user) ? kValid : kInvalid);

        if (state == kIntermediate)
            return QValidator::Intermediate;
    }

    return state == kValid
        ? QValidator::Acceptable
        : QValidator::Invalid;
}

QString QnSubjectValidationPolicy::calculateAlert(
    bool allUsers, const QSet<QnUuid>& subjects) const
{
    return !allUsers && subjects.empty() && !m_allowEmptySelection
        ? kForceHtml.arg(nx::vms::event::StringsHelper::needToSelectUserText())
        : QString();
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// QnDefaultSubjectValidationPolicy

QnDefaultSubjectValidationPolicy::QnDefaultSubjectValidationPolicy(bool allowEmptySelection) :
    base_type(allowEmptySelection)
{
}

QValidator::State QnDefaultSubjectValidationPolicy::roleValidity(const QnUuid& /*roleId*/) const
{
    return QValidator::Acceptable;
}

bool QnDefaultSubjectValidationPolicy::userValidity(const QnUserResourcePtr& /*user*/) const
{
    return true;
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// QnRequiredPermissionSubjectPolicy

QnRequiredPermissionSubjectPolicy::QnRequiredPermissionSubjectPolicy(
    Qn::GlobalPermission requiredPermission,
    const QString& permissionName,
    bool allowEmptySelection)
    :
    base_type(allowEmptySelection),
    m_requiredPermission(requiredPermission),
    m_permissionName(permissionName)
{
}

bool QnRequiredPermissionSubjectPolicy::userValidity(const QnUserResourcePtr& user) const
{
    return resourceAccessManager()->hasGlobalPermission(user, m_requiredPermission);
}

QValidator::State QnRequiredPermissionSubjectPolicy::roleValidity(const QnUuid& roleId) const
{
    return isRoleValid(roleId) ? QValidator::Acceptable : QValidator::Invalid;
}

bool QnRequiredPermissionSubjectPolicy::isRoleValid(const QnUuid& roleId) const
{
    const auto role = userRolesManager()->predefinedRole(roleId);
    switch (role)
    {
        case Qn::UserRole::CustomPermissions:
        {
            NX_ASSERT(false); //< Should never happen.
            return false;
        }

        case Qn::UserRole::CustomUserRole:
        {
            const auto customRole = userRolesManager()->userRole(roleId);
            return customRole.permissions.testFlag(m_requiredPermission);
        }

        default:
        {
            const auto permissions = userRolesManager()->userRolePermissions(role);
            return permissions.testFlag(m_requiredPermission);
        }
    }
}

QString QnRequiredPermissionSubjectPolicy::calculateAlert(bool allUsers,
    const QSet<QnUuid>& subjects) const
{
    QString alert = base_type::calculateAlert(allUsers, subjects);
    if (!alert.isEmpty())
        return alert;

    QVector<QnUuid> validRoles;
    QVector<QnUuid> invalidRoles;
    QVector<QnUuid> intermediateRoles;
    QVector<QnUserResourcePtr> validUsers;
    QVector<QnUserResourcePtr> invalidUsers;

    analyze(allUsers, subjects,
        validRoles, invalidRoles, intermediateRoles,
        validUsers, invalidUsers);

    NX_EXPECT(intermediateRoles.empty()); //< Unused in this policy.

    if (invalidRoles.size() > 0)
    {
        if (invalidRoles.size() == 1)
        {
            alert = tr("Role %1 has no %2 permission",
                "%1 is the name of selected role, %2 is permission name")
                .arg(lit("<b>%1</b>").arg(userRolesManager()->userRoleName(invalidRoles.front())))
                .arg(m_permissionName);
        }
        else if (validRoles.empty())
        {
            alert = tr("Selected roles have no %1 permission", "%1 is permission name")
                .arg(m_permissionName);
        }
        else
        {
            alert = tr("%n of %1 selected roles have no %2 permission",
                "%1 is number of selected roles, %2 is permission name", invalidRoles.size())
                .arg(validRoles.size() + invalidRoles.size()).arg(m_permissionName);
        }
    }

    if (invalidUsers.size() > 0)
    {
        if (!alert.isEmpty())
            alert += lit("<br>");

        if (invalidUsers.size() == 1)
        {
            alert += tr("User %1 has no %2 permission",
                "%1 is the name of selected user, %2 is permission name")
                .arg(lit("<b>%1</b>").arg(invalidUsers.front()->getName()))
                .arg(m_permissionName);
        }
        else if (validUsers.empty())
        {
            alert += tr("Selected users have no %1 permission", "%1 is permission name")
                .arg(m_permissionName);
        }
        else
        {
            alert += tr("%n of %1 selected users have no %2 permission",
                "%1 is number of selected users, %2 is permission name", invalidUsers.size())
                .arg(validUsers.size() + invalidUsers.size()).arg(m_permissionName);
        }
    }

    return alert.isEmpty() ? alert : kForceHtml.arg(alert);
}
