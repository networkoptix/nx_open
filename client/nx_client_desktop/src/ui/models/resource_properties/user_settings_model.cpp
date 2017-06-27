#include "user_settings_model.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource_access/shared_resources_manager.h>
#include <core/resource_access/resource_access_filter.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>

#include <nx/client/desktop/ui/actions/action_manager.h>
#include <nx/client/desktop/ui/actions/action_parameters.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>

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

    auto calculateMode = [this]
        {
            if (!m_user)
                return Invalid;

            if (m_user == context()->user())
                return OwnProfile;

            if (m_user->flags().testFlag(Qn::local))
                return NewUser;

            if (!accessController()->hasPermissions(m_user, Qn::WriteAccessRightsPermission))
                return OtherProfile;

            return OtherSettings;
        };

    m_mode = calculateMode();

    updatePermissions();
    emit userChanged(m_user);
}

void QnUserSettingsModel::updatePermissions()
{
    m_accessibleResources = m_user && m_mode != NewUser
        ? sharedResourcesManager()->sharedResources(m_user)
        : QSet<QnUuid>();
}

Qn::GlobalPermissions QnUserSettingsModel::rawPermissions() const
{
    if (!m_user)
        return Qn::NoGlobalPermissions;

    return m_user->getRawPermissions();
}

void QnUserSettingsModel::setRawPermissions(Qn::GlobalPermissions value)
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
