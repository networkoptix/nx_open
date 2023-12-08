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
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_access/resource_access_subject_hierarchy.h>
#include <core/resource_management/resource_pool.h>
#include <nx/branding.h>
#include <nx/network/app_info.h>
#include <nx/utils/qt_helpers.h>
#include <nx/vms/api/analytics/descriptors.h>
#include <nx/vms/api/analytics/engine_manifest.h>
#include <nx/vms/client/desktop/access/access_controller.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/resource_properties/user/utils/access_rights_list.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/user_management/user_management_helpers.h>
#include <nx/vms/event/action_parameters.h>
#include <nx/vms/event/events/abstract_event.h>
#include <nx/vms/event/strings_helper.h>
#include <utils/email/email.h>

using namespace nx::vms::api;
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
// QnAllowAnyCameraPolicy
//-------------------------------------------------------------------------------------------------

QString QnAllowAnyCameraPolicy::getText(const QnResourceList& resources, const bool detailed)
{
    QnVirtualCameraResourceList cameras = resources.filtered<QnVirtualCameraResource>();
    return genericCameraText<QnAllowAnyCameraPolicy>(cameras, detailed, "", /* invalid */ 0);
}

//-------------------------------------------------------------------------------------------------
// QnAtleastOneCameraPolicy
//-------------------------------------------------------------------------------------------------

QString QnRequireCameraPolicy::getText(const QnResourceList& resources, const bool detailed)
{
    QnVirtualCameraResourceList cameras = resources.filtered<QnVirtualCameraResource>();
    return genericCameraText<QnRequireCameraPolicy>(cameras, detailed, "", /* invalid */ 0);
}

//-------------------------------------------------------------------------------------------------
// QnCameraInputPolicy
//-------------------------------------------------------------------------------------------------

bool QnCameraInputPolicy::isResourceValid(const QnVirtualCameraResourcePtr &camera)
{
    return camera->hasCameraCapabilities(DeviceCapability::inputPort);
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
    return camera->hasCameraCapabilities(DeviceCapability::outputPort);
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
    if (cameras.empty())
        return QnBusinessResourceValidationStrings::selectCamera();

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
    return userGroupManager()->contains(resourceId);
}

bool QnSendEmailActionDelegate::isValidList(const QSet<QnUuid>& ids, const QString& additional)
{
    auto context = appContext()->currentSystemContext();

    QnUserResourceList users;
    QList<QnUuid> groupIds;
    nx::vms::common::getUsersAndGroups(context, ids, users, groupIds);

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

    const auto groupUsers = context->accessSubjectHierarchy()->usersInGroups(
        nx::utils::toQSet(groupIds));
    if (!std::all_of(groupUsers.cbegin(), groupUsers.cend(), isValidUser))
        return false;

    return countEnabledUsers(users) != 0 || !groupIds.empty() || !additionalRecipients.empty();
}

QString QnSendEmailActionDelegate::getText(const QSet<QnUuid>& ids, const bool detailed,
    const QString& additionalList)
{
    auto context = appContext()->currentSystemContext();

    QnUserResourceList users;
    UserGroupDataList groups;
    nx::vms::common::getUsersAndGroups(context, ids, users, groups);
    users = users.filtered([](const QnUserResourcePtr& user) { return user->isEnabled(); });

    auto additional = parseAdditional(additionalList);

    if (users.empty() && groups.empty() && additional.isEmpty())
        return nx::vms::event::StringsHelper::needToSelectUserText();

    if (!detailed)
    {
        QStringList recipients;
        if (!users.empty() || !groups.empty())
        {
            recipients << nx::vms::event::StringsHelper(context)
                .actionSubjects(users, groups, true);
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
    QSet<QnUuid> groupIds;

    for (const auto& group: groups)
    {
        groupIds.insert(group.id);
        receivers.push_back(group.name);
    }

    for (const auto& user: context->accessSubjectHierarchy()->usersInGroups(groupIds))
    {
        if (!user || !user->isEnabled())
            continue;

        ++total;
        if (!isValidUser(user))
            invalidUsers << user;
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

    const auto userGroups = nx::vms::common::userGroupsWithParents(user);
    for (const auto& groupId: userGroups)
    {
        if (std::find(subjects.cbegin(), subjects.cend(), groupId) != subjects.cend())
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
    QList<QnUuid> groupIds;

    if (allUsers)
        users = resourcePool()->getResources<QnUserResource>();
    else
        nx::vms::common::getUsersAndGroups(systemContext(), subjects, users, groupIds);

    for (const auto& id: groupIds)
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

bool QnSubjectValidationPolicy::isEmptySelectionAllowed() const
{
    return m_allowEmptySelection;
}

// This function is generally faster than QnSubjectValidationPolicy::analyze.
QValidator::State QnSubjectValidationPolicy::validity(bool allUsers,
    const QSet<QnUuid>& subjects) const
{
    if (!allUsers && subjects.empty() && !m_allowEmptySelection)
        return QValidator::Invalid;

    QnUserResourceList users;
    QList<QnUuid> groupIds;

    if (allUsers)
        users = resourcePool()->getResources<QnUserResource>();
    else
        nx::vms::common::getUsersAndGroups(systemContext(), subjects, users, groupIds);

    enum StateFlag
    {
        kInvalid = 0x1,
        kValid = 0x2,
        kIntermediate = kInvalid | kValid
    };

    int state = 0;

    for (const auto& id: groupIds)
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
// QnRequiredAccessRightPolicy

QnRequiredAccessRightPolicy::QnRequiredAccessRightPolicy(
    AccessRight requiredAccessRight,
    bool allowEmptySelection)
    :
    base_type(allowEmptySelection),
    m_requiredAccessRight(requiredAccessRight)
{
}

bool QnRequiredAccessRightPolicy::userValidity(const QnUserResourcePtr& user) const
{
    return isSubjectValid(user);
}

QValidator::State QnRequiredAccessRightPolicy::roleValidity(const QnUuid& groupId) const
{
    const auto group = userGroupManager()->find(groupId);
    if (!group)
        return QValidator::Invalid;

    return isSubjectValid(*group) ? QValidator::Acceptable : QValidator::Invalid;
}

QnSharedResourcePointerList<QnVirtualCameraResource> QnRequiredAccessRightPolicy::cameras() const
{
    return m_cameras;
}

void QnRequiredAccessRightPolicy::setCameras(
    const QnSharedResourcePointerList<QnVirtualCameraResource>& cameras)
{
    m_cameras = cameras;
}

bool QnRequiredAccessRightPolicy::isSubjectValid(const QnResourceAccessSubject& subject) const
{
    const auto hasPermissionForCamera =
        [=](const QnVirtualCameraResourcePtr& camera)
        {
            return resourceAccessManager()->hasAccessRights(
                subject, camera, m_requiredAccessRight);
        };

    if (!m_cameras.isEmpty())
        return std::all_of(m_cameras.begin(), m_cameras.end(), hasPermissionForCamera);

    if (resourceAccessManager()->hasAccessRights(
        subject, kAllDevicesGroupId, m_requiredAccessRight))
    {
        return true;
    }

    const auto& cameras = resourcePool()->getAllCameras();
    return std::any_of(cameras.begin(), cameras.end(), hasPermissionForCamera);
}

QString QnRequiredAccessRightPolicy::calculateAlert(bool allUsers,
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

    const auto permissionName = AccessRightsList::instance()->get(m_requiredAccessRight).name;

    if (invalidRoles.size() > 0)
    {
        if (invalidRoles.size() == 1)
        {
            const auto group = userGroupManager()->find(invalidRoles.front()).value_or(
                UserGroupData{});

            alert = tr("User group %1 has no %2 permission",
                "%1 is the name of selected user group, %2 is permission name")
                .arg(html::bold(group.name))
                .arg(permissionName);
        }
        else if (validRoles.empty())
        {
            alert = tr("Selected user groups have no %1 permission", "%1 is permission name")
                .arg(permissionName);
        }
        else
        {
            alert = tr("%n of %1 selected user groups have no %2 permission",
                "%1 is number of selected user groups, %2 is permission name", invalidRoles.size())
                .arg(validRoles.size() + invalidRoles.size()).arg(permissionName);
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
                .arg(permissionName);
        }
        else if (validUsers.empty())
        {
            alert += tr("Selected users have no %1 permission", "%1 is permission name")
                .arg(permissionName);
        }
        else
        {
            alert += tr("%n of %1 selected users have no %2 permission",
                "%1 is number of selected users, %2 is permission name", invalidUsers.size())
                .arg(validUsers.size() + invalidUsers.size()).arg(permissionName);
        }
    }

    return alert.isEmpty() ? alert : html::document(alert);
}

//-------------------------------------------------------------------------------------------------
// QnLayoutAccessValidationPolicy

QValidator::State QnLayoutAccessValidationPolicy::roleValidity(const QnUuid& roleId) const
{
    const auto group = userGroupManager()->find(roleId);
    if (!group)
        return QValidator::Invalid;

    if (m_layout)
    {
        // Admins have access to all layouts.
        if (kAllPowerUserGroupIds.contains(roleId))
            return QValidator::Acceptable;

        // For other users access permissions depend on the layout kind.
        if (m_layout->isShared())
        {
            // Shared layout. Ask AccessManager for details.
            if (resourceAccessManager()->hasPermission(*group, m_layout, Qn::ReadPermission))
                return QValidator::Acceptable;

            const auto groupUsers = accessSubjectHierarchy()->usersInGroups({roleId});
            if (std::any_of(groupUsers.cbegin(), groupUsers.cend(),
                [this](const auto& user) { return user->isEnabled() && userValidity(user); }))
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
        ? resourceAccessManager()->hasPermission(user, m_layout, Qn::ReadPermission)
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
    const auto& users = accessSubjectHierarchy()->usersInGroups({roleId});
    bool hasCloud = false;
    bool hasNonCloud = false;

    for (const auto& user: users)
    {
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
            QnUuidList groupIds;
            nx::vms::common::getUsersAndGroups(systemContext(), subjects, users, groupIds);

            for (const auto& user: accessSubjectHierarchy()->usersInGroups(
                nx::utils::toQSet(groupIds)))
            {
                if (std::find_if(users.begin(), users.end(),
                    [user](const QnUserResourcePtr& existing)
                    {
                        return user->getId() == existing->getId();
                    }) == users.end())
                {
                    users << user;
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

//-------------------------------------------------------------------------------------------------
// QnUserWithEmailValidationPolicy

QValidator::State QnUserWithEmailValidationPolicy::roleValidity(const QnUuid& roleId) const
{
    bool hasValid{false};
    bool hasInvalid{false};
    for (const auto& user: systemContext()->accessSubjectHierarchy()->usersInGroups({roleId}))
    {
        if (userValidity(user))
        {
            hasValid = true;
        }
        else
        {
            if (hasValid)
                return QValidator::State::Intermediate;

            hasInvalid = true;
        }
    }

    return hasInvalid ? QValidator::State::Invalid : QValidator::State::Acceptable;
}

bool QnUserWithEmailValidationPolicy::userValidity(const QnUserResourcePtr& user) const
{
    // Disabled users are just not displayed.
    if (!user->isEnabled())
        return true;

    return nx::email::isValidAddress(user->getEmail());
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
