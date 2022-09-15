// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "business_resource_validation.h"

#include <algorithm>

#include <QtCore/QCryptographicHash>
#include <QtWidgets/QLayout>

#include <client_core/client_core_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subjects_cache.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/branding.h>
#include <nx/network/app_info.h>
#include <nx/vms/api/analytics/descriptors.h>
#include <nx/vms/api/analytics/engine_manifest.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/event/action_parameters.h>
#include <nx/vms/event/events/abstract_event.h>
#include <nx/vms/event/strings_helper.h>
#include <utils/email/email.h>

using namespace nx::vms::client::desktop;

namespace {

QString braced(const QString& source)
{
    return '<' + source + '>';
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
            qnClientCoreModule->resourcePool(),
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
            qnClientCoreModule->resourcePool(),
            tr("Any Device"),
            tr("Any Camera")
        ));
    }

    static QString selectCamera()
    {
        return QnDeviceDependentStrings::getDefaultNameFromSet(
            qnClientCoreModule->resourcePool(),
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
        qnClientCoreModule->resourcePool(),
        cameras);
}

}

//-------------------------------------------------------------------------------------------------
// QnCameraInputPolicy
//-------------------------------------------------------------------------------------------------

bool QnCameraInputPolicy::isResourceValid(const QnVirtualCameraResourcePtr &camera)
{
    return camera->hasCameraCapabilities(nx::vms::api::DeviceCapability::inputPort);
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
    return camera->hasCameraCapabilities(nx::vms::api::DeviceCapability::outputPort);
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
    return camera->isScheduleEnabled() && camera->isMotionDetectionSupported();
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
    return camera->hasTwoWayAudio();
}

QString QnCameraAudioTransmitPolicy::getText(const QnResourceList &resources, const bool detailed)
{
    QnVirtualCameraResourceList cameras = resources.filtered<QnVirtualCameraResource>();
    int invalid = invalidResourcesCount<QnCameraAudioTransmitPolicy>(cameras);
    if (cameras.isEmpty())
        return QnDeviceDependentStrings::getDefaultNameFromSet(
            qnClientCoreModule->resourcePool(),
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
    return camera->isScheduleEnabled();
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
    return !camera->compatibleAnalyticsEngines().isEmpty();
}

QString QnCameraAnalyticsPolicy::getText(const QnResourceList& resources, const bool detailed)
{
    const auto cameras = resources.filtered<QnVirtualCameraResource>();

    int invalid = invalidResourcesCount<QnCameraAnalyticsPolicy>(cameras);
    return genericCameraText<QnCameraAnalyticsPolicy>(cameras, detailed,
        tr("Analytics is not available for %1"), invalid);
}

//-------------------------------------------------------------------------------------------------
// QnFullscreenCameraPolicy
//-------------------------------------------------------------------------------------------------

bool QnFullscreenCameraPolicy::isResourceValid(const QnVirtualCameraResourcePtr& camera)
{
    return true;
}

QString QnFullscreenCameraPolicy::getText(const QnResourceList& resources, const bool /*detailed*/)
{
    const auto cameras = resources.filtered<QnVirtualCameraResource>();
    if (cameras.size() != 1)
        return tr("Select exactly one camera");

    return getShortResourceName(cameras.first());
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
    return !userRolesManager()->userRole(resourceId).id.isNull()
        || userRolesManager()->predefinedRole(resourceId) != Qn::UserRole::customUserRole;
}

bool QnSendEmailActionDelegate::isValidList(const QSet<QnUuid>& ids, const QString& additional)
{
    auto context = appContext()->currentSystemContext();

    QnUserResourceList users;
    QList<QnUuid> roles;
    context->userRolesManager()->usersAndRoles(ids, users, roles);

    if (!std::all_of(users.cbegin(), users.cend(), &isValidUser))
        return false;

    const auto additionalRecipients = parseAdditional(additional);
    if (!std::all_of(
        additionalRecipients.cbegin(),
        additionalRecipients.cend(),
        nx::email::isValidAddress))
    {
        return false;
    }

    for (const auto& roleId: roles)
    {
        const auto isValidSubject =
            [isValid = &isValidUser](const QnResourceAccessSubject& subject)
            {
                return isValid(subject.user());
            };

        const auto usersInRole = context->resourceAccessSubjectsCache()->allUsersInRole(roleId);
        if (!std::all_of(usersInRole.cbegin(), usersInRole.cend(), isValidSubject))
            return false;
    }

    return countEnabledUsers(users) != 0 || !roles.empty() || !additionalRecipients.empty();
}

QString QnSendEmailActionDelegate::getText(const QSet<QnUuid>& ids, const bool detailed,
    const QString& additionalList)
{
    auto module = appContext()->currentSystemContext();

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
            recipients << nx::vms::event::StringsHelper(module)
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
        for (const auto& subject: module->resourceAccessSubjectsCache()->allUsersInRole(roleId))
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
    for (auto email: additional.split(';', Qt::SkipEmptyParts))
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
bool actionAllowedForUser(const nx::vms::event::AbstractActionPtr& action,
    const QnUserResourcePtr& user)
{
    if (!user)
        return false;

    switch (action->actionType())
    {
        case nx::vms::event::ActionType::fullscreenCameraAction:
        case nx::vms::event::ActionType::exitFullscreenAction:
            return true;
        default:
            break;
    }

    const auto params = action->getParams();
    if (params.allUsers)
        return true;

    const auto userId = user->getId();
    const auto& subjects = params.additionalResources;

    if (std::find(subjects.cbegin(), subjects.cend(), userId) != subjects.cend())
        return true;

    for (const auto& roleId: user->allUserRoleIds())
    {
        if (std::find(subjects.cbegin(), subjects.cend(), roleId) != subjects.cend())
            return true;
    }
    return false;
}

} // namespace QnBusiness

//-------------------------------------------------------------------------------------------------
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
        ? nx::vms::common::html::document(nx::vms::event::StringsHelper::needToSelectUserText())
        : QString();
}

//-------------------------------------------------------------------------------------------------
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

//-------------------------------------------------------------------------------------------------
// QnRequiredPermissionSubjectPolicy

QnRequiredPermissionSubjectPolicy::QnRequiredPermissionSubjectPolicy(
    GlobalPermission requiredPermission,
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
        case Qn::UserRole::customPermissions:
        {
            NX_ASSERT(false); //< Should never happen.
            return false;
        }

        case Qn::UserRole::customUserRole:
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
    using namespace nx::vms::common;

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

    NX_ASSERT(intermediateRoles.empty()); //< Unused in this policy.

    if (invalidRoles.size() > 0)
    {
        if (invalidRoles.size() == 1)
        {
            alert = tr("Role %1 has no %2 permission",
                "%1 is the name of selected role, %2 is permission name")
                .arg(html::bold(userRolesManager()->userRoleName(invalidRoles.front())))
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
            alert += html::kLineBreak;

        if (invalidUsers.size() == 1)
        {
            alert += tr("User %1 has no %2 permission",
                "%1 is the name of selected user, %2 is permission name")
                .arg(html::bold(invalidUsers.front()->getName()))
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

    return alert.isEmpty() ? alert : html::document(alert);
}

//-------------------------------------------------------------------------------------------------
// QnLayoutAccessValidationPolicy

QnLayoutAccessValidationPolicy::QnLayoutAccessValidationPolicy(SystemContext* systemContext):
    m_accessManager(systemContext->resourceAccessManager()),
    m_rolesManager(systemContext->userRolesManager())
{
}

QValidator::State QnLayoutAccessValidationPolicy::roleValidity(const QnUuid& roleId) const
{
    if (m_layout)
    {
        // Admins have access to all layouts.
        if (QnUserRolesManager::adminRoleIds().contains(roleId))
            return QValidator::Acceptable;

        // For other users access permissions depend on the layout kind.
        if (m_layout->isShared())
        {
            // Shared layout. Ask AccessManager for details.
            if (m_accessManager->hasPermission(
                m_rolesManager->userRole(roleId), m_layout, Qn::ReadPermission))
            {
                return QValidator::Acceptable;
            }

            auto usersInRole = resourceAccessSubjectsCache()->allUsersInRole(roleId);
            if (std::any_of(usersInRole.begin(), usersInRole.end(),
                [this](const auto& subject)
                {
                    const auto& user = resourcePool()->getResourceById<QnUserResource>(subject.id());
                    return user->isEnabled() && userValidity(user);
                }))
            {
                return QValidator::Intermediate;
            }

            return QValidator::Invalid;
        }
        else
        {
            // Local layout. Non-admin groups have no access.
            return QValidator::Invalid;
        }
    }

    // No layout has been selected. All users are acceptable.
    return QValidator::Acceptable;
}

bool QnLayoutAccessValidationPolicy::userValidity(const QnUserResourcePtr& user) const
{
    return m_layout
        ? m_accessManager->hasPermission(user, m_layout, Qn::ReadPermission)
        : true;
}

void QnLayoutAccessValidationPolicy::setLayout(const QnLayoutResourcePtr& layout)
{
    m_layout = layout;
}

//-------------------------------------------------------------------------------------------------
// QnCloudUsersValidationPolicy

QValidator::State QnCloudUsersValidationPolicy::roleValidity(const QnUuid& roleId) const
{
    const auto& users = resourceAccessSubjectsCache()->allUsersInRole(roleId);
    bool hasCloud = false;
    bool hasNonCloud = false;

    for (const auto& subject: users)
    {
        if (!subject.isUser())
            continue;
        const auto& user = subject.user();

        if (!user->isEnabled())
            continue;

        hasCloud |= user->isCloud();
        hasNonCloud |= !user->isCloud();
    }

    return hasNonCloud ? QValidator::Intermediate : QValidator::Acceptable;
}

bool QnCloudUsersValidationPolicy::userValidity(const QnUserResourcePtr& user) const
{
    return user->isCloud();
}

QString QnCloudUsersValidationPolicy::calculateAlert(
    bool allUsers,
    const QSet<QnUuid>& subjects) const
{
    const auto buildUserList =
        [this](const QSet<QnUuid>& subjects) -> QnUserResourceList
        {
            QnUserResourceList users;
            QnUuidList roles;
            userRolesManager()->usersAndRoles(subjects, users, roles);

            for (const auto& roleId: roles)
            {
                const auto& inRole = resourceAccessSubjectsCache()->allUsersInRole(roleId);
                for (const auto& subject: inRole)
                {
                    if (!subject.isUser())
                        continue;

                    if (std::find_if(users.begin(), users.end(),
                        [subject](const QnUserResourcePtr& existing)
                        {
                            return subject.user()->getId() == existing->getId();
                        }) == users.end())
                    {
                        users << subject.user();
                    }
                }
            }

            return users;
        };

    const QnUserResourceList users = allUsers
        ? resourcePool()->getResources<QnUserResource>()
        : buildUserList(subjects);

    int nonCloudCount = 0, totalCount = 0;
    for (const auto& user: users)
    {
        if (!user->isEnabled())
            continue;

        if (!user->isCloud())
            ++nonCloudCount;

        ++totalCount;
    }

    if (totalCount == 0)
        return nx::vms::event::StringsHelper::needToSelectUserText();

    if (nonCloudCount != 0)
    {
        return tr("%n of %1 selected users are not %2 users and will not get mobile notifications.",
            "%2 here will be substituted with short cloud name e.g. 'Cloud'.",
            nonCloudCount)
            .arg(totalCount)
            .arg(nx::branding::shortCloudName());
    }

    return QString();
}

bool QnBuzzerPolicy::isServerValid(const QnMediaServerResourcePtr& server)
{
    if (!server)
        return false;

    using namespace nx::vms::api;
    return server->getServerFlags().testFlag(ServerFlag::SF_HasBuzzer);
}

QString QnBuzzerPolicy::infoText()
{
    return tr("Servers that support buzzer");
}

bool QnPoeOverBudgetPolicy::isServerValid(const QnMediaServerResourcePtr& server)
{
    if (!server)
        return false;

    using namespace nx::vms::api;
    return server->getServerFlags().testFlag(ServerFlag::SF_HasPoeManagementCapability);
}

QString QnPoeOverBudgetPolicy::infoText()
{
    return tr("Servers that support PoE monitoring");
}

bool QnFanErrorPolicy::isServerValid(const QnMediaServerResourcePtr& server)
{
    if (!server)
        return false;

    using namespace nx::vms::api;
    return server->getServerFlags().testFlag(ServerFlag::SF_HasFanMonitoringCapability);
}

QString QnFanErrorPolicy::infoText()
{
    return tr("Servers that support fan diagnostic");
}
