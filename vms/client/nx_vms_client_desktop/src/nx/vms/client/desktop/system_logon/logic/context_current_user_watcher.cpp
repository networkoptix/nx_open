// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "context_current_user_watcher.h"

#include <client/client_globals.h>
#include <client/client_message_processor.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/access_rights_manager.h>
#include <core/resource_access/global_permissions_watcher.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_access/resource_access_subject_hierarchy.h>
#include <core/resource_management/resource_pool.h>
#include <network/system_helpers.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/common/user_management/user_group_manager.h>
#include <utils/common/checked_cast.h>

namespace nx::vms::client::desktop {

ContextCurrentUserWatcher::ContextCurrentUserWatcher(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    connect(systemContext()->userWatcher(), &core::UserWatcher::userChanged, this,
        [this](const QnUserResourcePtr& user)
        {
            if (user)
            {
                setCurrentUser(user);
                return;
            }

            setCurrentUser({});

            if (this->connection())
                menu()->triggerForced(ui::action::DisconnectAction, {Qn::ForceRole, true});
        });

    connect(systemContext()->globalPermissionsWatcher(),
        &nx::core::access::GlobalPermissionsWatcher::ownGlobalPermissionsChanged,
        this,
        [this](const QnUuid& subjectId)
        {
            if (!m_user || m_user->hasFlags(Qn::removed))
                return;

            if (systemContext()->accessSubjectHierarchy()->isWithin(m_user->getId(), {subjectId}))
                reconnect();
        });

    connect(systemContext()->accessSubjectHierarchy(),
        &nx::core::access::SubjectHierarchy::changed, this,
        [this](
            const QSet<QnUuid>& /*added*/,
            const QSet<QnUuid>& /*removed*/,
            const QSet<QnUuid>& groupsWithChangedMembers,
            const QSet<QnUuid>& subjectsWithChangedParents)
        {
            if (!m_user || m_user->hasFlags(Qn::removed))
                return;

            if (!resourcePool()->getResourceById(m_user->getId()))
                return;

            if (groupsWithChangedMembers.empty() && subjectsWithChangedParents.empty())
                return;

            if (updateParentGroups())
                reconnect();
        });

    connect(systemContext()->userGroupManager(),
        &nx::vms::common::UserGroupManager::addedOrUpdated, this,
        [this](const nx::vms::api::UserGroupData&)
        {
            if (updateParentGroups())
                reconnect();
        });
}

ContextCurrentUserWatcher::~ContextCurrentUserWatcher()
{
}

bool ContextCurrentUserWatcher::updateParentGroups()
{
    const auto groups = allParentGroups();
    if (groups == m_allParentGroups)
        return false;

    m_allParentGroups = groups;

    return true;
}

QSet<QnUuid> ContextCurrentUserWatcher::allParentGroups() const
{
    if (!m_user)
        return {};

    const auto directParents = m_user->groupIds();
    QSet<QnUuid> parentIds = {directParents.begin(), directParents.end()};
    parentIds += systemContext()->accessSubjectHierarchy()->recursiveParents(parentIds);
    parentIds.insert(m_user->getId());

    return parentIds;
}

void ContextCurrentUserWatcher::setCurrentUser(const QnUserResourcePtr &user)
{
    if (m_user == user)
        return;

    if (m_user)
        m_user->disconnect(this);

    m_user = user;
    m_userPassword = QString();
    m_userDigest = QByteArray();

    if (m_user)
    {
        m_userDigest = m_user->getDigest();

        connect(m_user.get(), &QnResource::nameChanged, this,
            &ContextCurrentUserWatcher::at_user_resourceChanged);
        connect(m_user.get(), &QnUserResource::passwordChanged, this,
            &ContextCurrentUserWatcher::at_user_resourceChanged);
    }

    m_allParentGroups = allParentGroups();
}

void ContextCurrentUserWatcher::setUserPassword(const QString &password)
{
    if (m_userPassword == password)
        return;

    m_userPassword = password;
    m_userDigest = QByteArray(); //hash will be recalculated
}

const QString & ContextCurrentUserWatcher::userPassword() const
{
    return m_userPassword;
}

void ContextCurrentUserWatcher::setReconnectOnPasswordChange(bool value)
{
    m_reconnectOnPasswordChange = value;
    if (value && m_user && isReconnectRequired(m_user))
        reconnect();
}

bool ContextCurrentUserWatcher::isReconnectRequired(const QnUserResourcePtr &user)
{
    if (m_userName.isEmpty())
        return false;   //we are not connected

    // TODO: #sivanov Actually reconnect on renaming is required when using digest auth only. But
    // some functionality (like desktop camera) can stop working, so it's simplier to reconnect.
    if (user->getName().toLower() != m_userName.toLower())
        return true;

    // TODO: #sivanov This is needed to avoid reconnecting on merging systems. Should be improved.
    if (!m_reconnectOnPasswordChange)
        return false;

    const auto credentials = systemContext()->connectionCredentials();
    const bool bearerAuthIsUsed =
        (credentials.authToken.type == nx::network::http::AuthTokenType::bearer);

    // Reconnect is never needed when token auth is used.
    if (bearerAuthIsUsed)
    {
        m_userDigest = user->getDigest();
        m_userPassword = QString();
        return false;
    }

    if (m_userPassword.isEmpty())
        return m_userDigest != user->getDigest();

    // TODO: #sivanov Handle this on the server side: force disconnect users with changed password.
    // Filled m_userPassword means password was just changed by the user.
    if (!user->getHash().checkPassword(m_userPassword))
        return true;

    m_userDigest = user->getDigest();
    m_userPassword = QString();
    return false;
}

void ContextCurrentUserWatcher::reconnect()
{
    menu()->trigger(ui::action::ReconnectAction);
}

void ContextCurrentUserWatcher::at_user_resourceChanged(const QnResourcePtr &resource)
{
    if (isReconnectRequired(resource.dynamicCast<QnUserResource>()))
        reconnect();
}

} // namespace nx::vms::client::desktop
