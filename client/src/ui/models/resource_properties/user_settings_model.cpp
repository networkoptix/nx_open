#include "user_settings_model.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_access_manager.h>
#include <core/resource_management/resource_access_filter.h>
#include <core/resource/user_resource.h>

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

    return qnResourceAccessManager->accessibleResources(m_user->getId());
}

void QnUserSettingsModel::setAccessibleResources(const QSet<QnUuid>& value)
{
    if (!m_user)
        return;

    qnResourceAccessManager->setAccessibleResources(m_user->getId(), value);
}

QString QnUserSettingsModel::groupName() const
{
    if (!m_user)
        return QString();

    if (m_user->isOwner())
        return tr("Owner");

    Qn::GlobalPermissions permissions = qnResourceAccessManager->globalPermissions(m_user);
    if (permissions.testFlag(Qn::GlobalAdminPermission))
        return tr("Administrator");

    for (const ec2::ApiUserGroupData& group : qnResourceAccessManager->userGroups())
        if (group.id == m_user->userGroup())
            return group.name;

    if (permissions == Qn::GlobalAdvancedViewerPermissionSet)
        return tr("Advanced Viewer");

    if (permissions == Qn::GlobalViewerPermissionSet)
        return tr("Viewer");

    if (permissions == Qn::GlobalLiveViewerPermissionSet)
        return tr("Live Viewer");

    return tr("Custom Permissions");
}

QString QnUserSettingsModel::permissionsDescription() const
{
    if (!m_user)
        return QString();

    return permissionsDescription(m_user, m_user->userGroup());
}

QString QnUserSettingsModel::permissionsDescription(const QnUserResourcePtr& user, const QnUuid& groupId) const
{
    if (user->isOwner())
        return tr("Has access to whole system and can do everything.");

    auto permissions = qnResourceAccessManager->globalPermissions(user);
    if (permissions == Qn::GlobalAdminPermissionsSet)
        return tr("Has access to whole system and can manage it. Can create users.");

    for (const ec2::ApiUserGroupData& group : qnResourceAccessManager->userGroups())
        if (group.id == groupId)
            return getCustomPermissionsDescription(group.id, group.permissions);

    if (permissions == Qn::GlobalAdvancedViewerPermissionSet)
        return tr("Can manage all cameras and bookmarks.");

    if (permissions == Qn::GlobalViewerPermissionSet)
        return tr("Can view all cameras and export video.");

    if (permissions == Qn::GlobalLiveViewerPermissionSet)
        return tr("Can view live video from all cameras.");

    if (m_user)
        return getCustomPermissionsDescription(user->getId(), permissions);

    return QString();
}

QString QnUserSettingsModel::getCustomPermissionsDescription(const QnUuid& id, Qn::GlobalPermissions permissions) const
{
    const bool hasAccessToSystem = accessController()->hasGlobalPermission(Qn::GlobalAdminPermission);

    QStringList tableRows;

    auto allResources = qnResPool->getResources();
    auto accessibleResources = qnResourceAccessManager->accessibleResources(id);

    std::map<QnResourceAccessFilter::Filter, QString> nameByFilter
    {
        {QnResourceAccessFilter::CamerasFilter, tr("Cameras")},
        {QnResourceAccessFilter::LayoutsFilter, tr("Layouts")}
    };

    for (auto filter : QnResourceAccessFilter::allFilters())
    {
        if (filter == QnResourceAccessFilter::CamerasFilter && permissions.testFlag(Qn::GlobalAccessAllCamerasPermission))
        {
            QString row = lit("<td colspan=2><b>%1</b></td>").arg(tr("All Cameras"));
            tableRows << kHtmlTableRowTemplate.arg(row);
        }
        else
        {
            auto allFiltered = QnResourceAccessFilter::filteredResources(filter, allResources);
            auto accessibleFiltered = QnResourceAccessFilter::filteredResources(filter, accessibleResources);

            QString row = hasAccessToSystem
                ? lit("<td><b>%1</b> / %2</td><td>%3</td>").arg(accessibleFiltered.size()).arg(allFiltered.size()).arg(nameByFilter[filter])
                : lit("<td><b>%1</b></td><td>%2</td>").arg(accessibleFiltered.size()).arg(nameByFilter[filter]);
            tableRows << kHtmlTableRowTemplate.arg(row);
        }
    }

    return kHtmlTableTemplate.arg(tableRows.join(QString()));
}
