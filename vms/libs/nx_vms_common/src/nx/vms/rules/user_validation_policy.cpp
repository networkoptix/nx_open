// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_validation_policy.h"

#include <api/helpers/access_rights_helper.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/user_management/user_management_helpers.h>
#include <nx/vms/event/strings_helper.h>
#include <utils/email/email.h>

using namespace nx::vms::api;

namespace {

QnUserResourceList buildUserList(
    const QSet<nx::Uuid>& subjects, nx::vms::common::SystemContext* context)
{
    QnUserResourceList users;
    QSet<nx::Uuid> groupIds;
    nx::vms::common::getUsersAndGroups(context, subjects, users, groupIds);

    for (const auto& user: context->accessSubjectHierarchy()->usersInGroups(
        groupIds, /*withHidden*/ false))
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
}

QnUserResourceList allVisibleUsers(QnResourcePool* resourcePool)
{
    return resourcePool->getResources<QnUserResource>(
        [](const QnUserResourcePtr& user) { return !nx::vms::common::isUserHidden(user); });
}

} // namespace

//-------------------------------------------------------------------------------------------------
// QnSubjectValidationPolicy

QnSubjectValidationPolicy::QnSubjectValidationPolicy(
    nx::vms::common::SystemContext* systemContext,
    bool allowEmptySelection)
    :
    SystemContextAware(systemContext),
    m_allowEmptySelection(allowEmptySelection)
{
}

QnSubjectValidationPolicy::~QnSubjectValidationPolicy()
{
}

void QnSubjectValidationPolicy::analyze(
    bool allUsers,
    const QSet<nx::Uuid>& subjects,
    QVector<nx::Uuid>& validRoles,
    QVector<nx::Uuid>& invalidRoles,
    QVector<nx::Uuid>& intermediateRoles,
    QVector<QnUserResourcePtr>& validUsers,
    QVector<QnUserResourcePtr>& invalidUsers) const
{
    validRoles.clear();
    invalidRoles.clear();
    intermediateRoles.clear();
    validUsers.clear();
    invalidUsers.clear();

    QnUserResourceList users;
    QSet<nx::Uuid> groupIds;

    if (allUsers)
        users = allVisibleUsers(resourcePool());
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
    const QSet<nx::Uuid>& subjects) const
{
    if (!allUsers && subjects.empty())
        return m_allowEmptySelection ? QValidator::Acceptable : QValidator::Invalid;

    QnUserResourceList users;
    QSet<nx::Uuid> groupIds;

    if (allUsers)
        users = allVisibleUsers(resourcePool());
    else
        nx::vms::common::getUsersAndGroups(systemContext(), subjects, users, groupIds);

    if (users.empty() && !groupIds.empty())
    {
        const auto usersInGroups = systemContext()->accessSubjectHierarchy()->usersInGroups(
            groupIds, /*withHidden*/ false);

        if (usersInGroups.empty())
            return QValidator::Intermediate;
    }

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
    bool allUsers, const QSet<nx::Uuid>& subjects) const
{
    if (allUsers)
        return {};

    if (subjects.empty() && !m_allowEmptySelection)
        return nx::vms::common::html::document(nx::vms::event::StringsHelper::needToSelectUserText());

    QnUserResourceList users;
    QSet<nx::Uuid> groupIds;
    nx::vms::common::getUsersAndGroups(systemContext(), subjects, users, groupIds);

    if (users.empty() && !groupIds.empty())
    {
        const auto usersInGroups = systemContext()->accessSubjectHierarchy()->usersInGroups(
            groupIds, /*withHidden*/ false);

        if (usersInGroups.empty())
            return nx::vms::common::html::document(tr("None of selected user roles contain users"));
    }

    return {};
}

//-------------------------------------------------------------------------------------------------
// QnDefaultSubjectValidationPolicy

QnDefaultSubjectValidationPolicy::QnDefaultSubjectValidationPolicy(
    nx::vms::common::SystemContext* systemContext,
    bool allowEmptySelection)
    :
    base_type(systemContext, allowEmptySelection)
{
}

QValidator::State QnDefaultSubjectValidationPolicy::roleValidity(const nx::Uuid& /*roleId*/) const
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
    nx::vms::common::SystemContext* systemContext,
    AccessRight requiredAccessRight,
    bool allowEmptySelection)
    :
    base_type(systemContext, allowEmptySelection),
    m_requiredAccessRight(requiredAccessRight)
{
}

bool QnRequiredAccessRightPolicy::userValidity(const QnUserResourcePtr& user) const
{
    return isSubjectValid(user);
}

QValidator::State QnRequiredAccessRightPolicy::roleValidity(const nx::Uuid& groupId) const
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

bool QnRequiredAccessRightPolicy::hasAnyCameraPermission(const QnUserResourcePtr& user) const
{
    return isSubjectValid(user, /* allSelectedCamerasRequired*/ false);
}

bool QnRequiredAccessRightPolicy::isSubjectValid(
    const QnResourceAccessSubject& subject, bool allSelectedCamerasRequired /* = true*/) const
{
    const auto hasPermissionForCamera =
        [this, subject](const QnVirtualCameraResourcePtr& camera)
        {
            return resourceAccessManager()->hasAccessRights(
                subject, camera, m_requiredAccessRight);
        };

    if (!m_cameras.isEmpty())
    {
        return allSelectedCamerasRequired
            ? std::all_of(m_cameras.begin(), m_cameras.end(), hasPermissionForCamera)
            : std::any_of(m_cameras.begin(), m_cameras.end(), hasPermissionForCamera);
    }

    if (resourceAccessManager()->hasAccessRights(
        subject, kAllDevicesGroupId, m_requiredAccessRight))
    {
        return true;
    }

    const auto& cameras = resourcePool()->getAllCameras();
    return std::any_of(cameras.begin(), cameras.end(), hasPermissionForCamera);
}

QString QnRequiredAccessRightPolicy::calculateAlert(bool allUsers,
    const QSet<nx::Uuid>& subjects) const
{
    using namespace nx::vms::common;

    QString alert = base_type::calculateAlert(allUsers, subjects);
    if (!alert.isEmpty())
        return alert;

    QVector<nx::Uuid> validRoles;
    QVector<nx::Uuid> invalidRoles;
    QVector<nx::Uuid> intermediateRoles;
    QVector<QnUserResourcePtr> validUsers;
    QVector<QnUserResourcePtr> invalidUsers;

    analyze(allUsers, subjects,
        validRoles, invalidRoles, intermediateRoles,
        validUsers, invalidUsers);

    NX_ASSERT(intermediateRoles.empty()); //< Unused in this policy.

    const auto permissionName = nx::vms::common::AccessRightHelper::name(m_requiredAccessRight);

    if (invalidRoles.size() > 0 && invalidUsers.size() > 0)
    {
        auto groups = tr("%n groups", "", invalidRoles.size());
        auto users = tr("%n users", "", invalidUsers.size());
        alert = tr(
            "%1 and %2 do not have %3 permission for some of selected cameras",
            "%1 and %2 are the numbers of user groups and users in a correct numeric form "
                "(e.g. '2 groups and 1 user'), %3 is the permission name")
                    .arg(groups).arg(users).arg(permissionName);
    }
    else if (invalidRoles.size() > 1)
    {
        alert = tr(
            "%n groups do not have %1 permission for some of selected cameras",
            "%1 is the permission name",
            invalidRoles.size())
                .arg(permissionName);
    }
    else if (invalidRoles.size() == 1)
    {
        const auto group = userGroupManager()->find(invalidRoles.front()).value_or(
            UserGroupData{});

        alert = tr(
            "%1 group does not have %2 permission for some of selected cameras",
            "%1 is the name of selected user group, %2 is the permission name")
                .arg(html::bold(group.name))
                .arg(permissionName);
    }
    else if (invalidUsers.size() > 1)
    {
        alert = tr(
            "%n users do not have %1 permission for some of selected cameras",
            "%1 is the permission name",
            invalidUsers.size())
                .arg(permissionName);
    }
    else if (invalidUsers.size() == 1)
    {
        const auto& user = invalidUsers.front();

        alert = tr(
            "%1 user does not have %2 permission for some of selected cameras",
            "%1 is the name of the selected user, %2 is the permission name")
                .arg(html::bold(user->getName()))
                .arg(permissionName);
    }

    return alert.isEmpty() ? alert : html::document(alert);
}

//-------------------------------------------------------------------------------------------------
// QnLayoutAccessValidationPolicy

QValidator::State QnLayoutAccessValidationPolicy::roleValidity(const nx::Uuid& roleId) const
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

            const auto groupUsers =
                systemContext()->accessSubjectHierarchy()->usersInGroups(
                    {roleId}, /*withHidden*/ false);

            if (std::any_of(groupUsers.cbegin(), groupUsers.cend(),
                [this](const auto& user) { return user->isEnabled() && userValidity(user); }))
            {
                // The group does not have read permissions, but some of the users in the group have.
                return QValidator::Intermediate;
            }

            return QValidator::Invalid;
        }

        // Local layout. Non-admin groups have no access.
        return QValidator::Invalid;
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

QString QnLayoutAccessValidationPolicy::calculateAlert(
    bool allUsers, const QSet<nx::Uuid>& subjects) const
{
    QString alert = base_type::calculateAlert(allUsers, subjects);
    if (!alert.isEmpty())
        return alert;

    const QnUserResourceList users = allUsers
        ? allVisibleUsers(resourcePool())
        : buildUserList(subjects, systemContext());

    int invalidUsersCount{};
    for (const auto& user: users)
    {
        if (!userValidity(user))
            ++invalidUsersCount;
    }

    if (invalidUsersCount == users.size())
        return tr("Users do not have access to the selected layout");

    if (invalidUsersCount > 0)
        return tr("Some users do not have access to the selected layout");

    return {};
}

void QnLayoutAccessValidationPolicy::setLayout(const QnLayoutResourcePtr& layout)
{
    m_layout = layout;
}

//-------------------------------------------------------------------------------------------------
// QnCloudUsersValidationPolicy

QValidator::State QnCloudUsersValidationPolicy::roleValidity(const nx::Uuid& roleId) const
{
    const auto& users = systemContext()->accessSubjectHierarchy()->usersInGroups(
        {roleId}, /*withHidden*/ false);

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
    const QSet<nx::Uuid>& subjects) const
{
    const QnUserResourceList users = allUsers
        ? allVisibleUsers(resourcePool())
        : buildUserList(subjects, systemContext());

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

QValidator::State QnUserWithEmailValidationPolicy::roleValidity(const nx::Uuid& roleId) const
{
    bool hasValid{false};
    bool hasInvalid{false};
    for (const auto& user: systemContext()->accessSubjectHierarchy()->usersInGroups(
        {roleId}, /*withHidden*/ false))
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

QString QnUserWithEmailValidationPolicy::calculateAlert(
    bool allUsers, const QSet<nx::Uuid>& subjects) const
{
    QString alert = base_type::calculateAlert(allUsers, subjects);
    if (!alert.isEmpty())
        return alert;

    const QnUserResourceList users = allUsers
        ? allVisibleUsers(resourcePool())
        : buildUserList(subjects, systemContext());

    int invalidUsersCount{};
    for (const auto& user: users)
    {
        if (!userValidity(user))
            ++invalidUsersCount;
    }

    if (invalidUsersCount == users.size())
        return tr("Email address is not set for all the selected users.");

    if (invalidUsersCount > 0)
        return tr("Email address is not set for some selected users.");

    return {};
}
