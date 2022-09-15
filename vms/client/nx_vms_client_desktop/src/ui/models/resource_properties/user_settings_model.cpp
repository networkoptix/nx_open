// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_settings_model.h"

#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_filter.h>
#include <core/resource_access/shared_resources_manager.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/action_parameters.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>

namespace
{
    const QString kHtmlTableTemplate(lit("<table>%1</table>"));
    const QString kHtmlTableRowTemplate(lit("<tr>%1</tr>"));
}

QnUserSettingsModel::QnUserSettingsModel(QObject* parent /*= nullptr*/) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_mode(Invalid),
    m_user()
{
}

QnUserSettingsModel::~QnUserSettingsModel()
{

}

QnUserSettingsModel::Mode QnUserSettingsModel::mode() const
{
    return m_mode;
}

QnUserResourcePtr QnUserSettingsModel::user() const
{
    return m_user;
}

void QnUserSettingsModel::setUser(const QnUserResourcePtr& value)
{
    if (m_user == value)
        return;

    m_user = value;

    updateMode();
    updatePermissions();

    m_configuredDigestSupport = (m_user && m_user->shouldDigestAuthBeUsed());

    emit userChanged(m_user);
}

void QnUserSettingsModel::updateMode()
{
    auto calculateMode =
        [this]
        {
            if (!m_user)
                return Invalid;

            if (m_user == context()->user())
                return OwnProfile;

            if (!m_user->resourcePool())
                return NewUser;

            if (!accessController()->hasPermissions(m_user, Qn::WriteAccessRightsPermission))
                return OtherProfile;

            return OtherSettings;
        };

    m_mode = calculateMode();
}

void QnUserSettingsModel::updatePermissions()
{
    m_accessibleResources = m_user && m_mode != NewUser
        ? sharedResourcesManager()->sharedResources(m_user)
        : QSet<QnUuid>();
}

bool QnUserSettingsModel::digestAuthorizationEnabled() const
{
    return m_configuredDigestSupport;
}

void QnUserSettingsModel::setDigestAuthorizationEnabled(bool enabled)
{
    NX_ASSERT(!m_user || !m_user->isCloud());

    if (m_configuredDigestSupport == enabled)
        return;

    m_configuredDigestSupport = enabled;
    emit digestSupportChanged();
}

QnUserResource::DigestSupport QnUserSettingsModel::digestSupport() const
{
    return ((m_user && m_user->shouldDigestAuthBeUsed()) == m_configuredDigestSupport)
        ? QnUserResource::DigestSupport::keep
        : m_configuredDigestSupport
            ? QnUserResource::DigestSupport::enable
            : QnUserResource::DigestSupport::disable;
}

GlobalPermissions QnUserSettingsModel::rawPermissions() const
{
    if (!m_user)
        return {};

    return m_user->getRawPermissions();
}

void QnUserSettingsModel::setRawPermissions(GlobalPermissions value)
{
    if (!m_user)
        return;

    m_user->setRawPermissions(value);
}

QSet<QnUuid> QnUserSettingsModel::accessibleResources() const
{
    if (!m_user)
        return QSet<QnUuid>();

    return m_accessibleResources;
}

void QnUserSettingsModel::setAccessibleResources(const QSet<QnUuid>& value)
{
    if (!m_user)
        return;

    m_accessibleResources = value;
}

QnResourceAccessSubject QnUserSettingsModel::subject() const
{
    return m_user;
}
