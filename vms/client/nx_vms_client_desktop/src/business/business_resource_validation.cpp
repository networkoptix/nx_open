// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "business_resource_validation.h"

#include <QtWidgets/QLayout>

#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/system_health/message_type.h>
#include <nx/vms/common/user_management/user_management_helpers.h>
#include <nx/vms/event/events/abstract_event.h>
#include <nx/vms/event/strings_helper.h>
#include <utils/email/email.h>

using namespace nx::vms::api;
using namespace nx::vms::client::desktop;

namespace {

int countEnabledUsers(const QnUserResourceList& users)
{
    return std::count_if(users.cbegin(), users.cend(),
        [](const QnUserResourcePtr& user) -> bool
        {
            return user && user->isEnabled();
        });
}

} // namespace

//-------------------------------------------------------------------------------------------------
// QnSendEmailActionDelegate
//-------------------------------------------------------------------------------------------------

QnSendEmailActionDelegate::QnSendEmailActionDelegate(
    SystemContext* systemContext,
    QWidget* parent)
    :
    QnResourceSelectionDialogDelegate(systemContext, parent),
    m_warningLabel(nullptr)
{
}

void QnSendEmailActionDelegate::init(QWidget* parent)
{
    m_warningLabel = new QLabel(parent);
    setWarningStyle(m_warningLabel);
    parent->layout()->addWidget(m_warningLabel);
}

QString QnSendEmailActionDelegate::validationMessage(const QSet<nx::Uuid>& selected) const
{
    const bool valid = isValidList(selected, QString()); // TODO: FIXME! Why additional is empty?
    return valid ? QString() : getText(selected, true, QString());
}

bool QnSendEmailActionDelegate::isValid(const nx::Uuid& resourceId) const
{
    if (resourceId.isNull()) //< custom permissions "Custom" role is not accepted.
        return false;

    if (auto user = resourcePool()->getResourceById<QnUserResource>(resourceId))
        return isValidUser(user);

    /* We can get here either user id or role id. User should be checked additionally, role is
     * always counted as valid (if exists). */
    return userGroupManager()->contains(resourceId);
}

bool QnSendEmailActionDelegate::isValidList(const QSet<nx::Uuid>& ids, const QString& additional)
{
    auto context = appContext()->currentSystemContext();

    QnUserResourceList users;
    QSet<nx::Uuid> groupIds;
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

QString QnSendEmailActionDelegate::getText(const QSet<nx::Uuid>& ids, const bool detailed,
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
    QSet<nx::Uuid> groupIds;

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
bool actionAllowedForUser(
    const nx::vms::event::AbstractActionPtr& action,
    const QnUserResourcePtr& user)
{
    if (!user)
        return false;

    const auto eventType = action->getRuntimeParams().eventType;
    if (eventType >= nx::vms::api::EventType::systemHealthEvent
        && eventType <= nx::vms::api::EventType::maxSystemHealthEvent)
    {
        const auto healthMessage = nx::vms::common::system_health::MessageType(
            eventType - nx::vms::api::EventType::systemHealthEvent);

        if (healthMessage == nx::vms::common::system_health::MessageType::showIntercomInformer
            || healthMessage == nx::vms::common::system_health::MessageType::showMissedCallInformer)
        {
            const auto& runtimeParameters = action->getRuntimeParams();

            const auto systemContext =
                qobject_cast<nx::vms::client::desktop::SystemContext*>(user->systemContext());
            if (!NX_ASSERT(systemContext))
                return false;

            const QnResourcePtr cameraResource =
                systemContext->resourcePool()->getResourceById(runtimeParameters.eventResourceId);

            const bool hasViewPermission = cameraResource &&
                systemContext->accessController()->hasPermissions(
                    cameraResource,
                    Qn::ViewContentPermission & Qn::UserInputPermissions);

            if (hasViewPermission)
                return true;
        }
    }

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
