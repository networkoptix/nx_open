#include "user_grous_settings_model.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_access_manager.h>
#include <core/resource/user_resource.h>

#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>

namespace
{
    const QString kHtmlTableTemplate(lit("<table>%1</table>"));
    const QString kHtmlTableRowTemplate(lit("<tr>%1</tr>"));


}

QnUserGroupSettingsModel::QnUserGroupSettingsModel(QObject* parent /*= nullptr*/) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_userGroupId()
{

}

QnUserGroupSettingsModel::~QnUserGroupSettingsModel()
{

}

QnUuid QnUserGroupSettingsModel::userGroupId() const
{
    return m_userGroupId;
}

void QnUserGroupSettingsModel::setUserGroupId(const QnUuid& value)
{
    m_userGroupId = value;
}

Qn::GlobalPermissions QnUserGroupSettingsModel::rawPermissions() const
{
    if (m_userGroupId.isNull())
        return Qn::NoGlobalPermissions;

    auto groups = qnResourceAccessManager->userGroups();
    auto group = std::find_if(groups.cbegin(), groups.cend(), [this](const ec2::ApiUserGroupData& elem) { return elem.id == m_userGroupId; });
    if (group == groups.cend())
        return Qn::NoGlobalPermissions;
    return group->permissions;
}

void QnUserGroupSettingsModel::setRawPermissions(Qn::GlobalPermissions value)
{
    if (m_userGroupId.isNull())
        return;

    auto groups = qnResourceAccessManager->userGroups();
    auto group = std::find_if(groups.begin(), groups.end(), [this](const ec2::ApiUserGroupData& elem) { return elem.id == m_userGroupId; });
    if (group == groups.end())
        return;
    (*group).permissions = value;
    qnResourceAccessManager->resetUserGroups(groups);
}

QSet<QnUuid> QnUserGroupSettingsModel::accessibleResources() const
{
    if (m_userGroupId.isNull())
        return QSet<QnUuid>();

    return qnResourceAccessManager->accessibleResources(m_userGroupId);
}

void QnUserGroupSettingsModel::setAccessibleResources(const QSet<QnUuid>& value)
{
    if (m_userGroupId.isNull())
        return;

    qnResourceAccessManager->setAccessibleResources(m_userGroupId, value);
}

QString QnUserGroupSettingsModel::groupName() const
{
    if (m_userGroupId.isNull())
        return QString();

    auto groups = qnResourceAccessManager->userGroups();
    auto group = std::find_if(groups.cbegin(), groups.cend(), [this](const ec2::ApiUserGroupData& elem) { return elem.id == m_userGroupId;  });
    if (group == groups.cend())
        return QString();
    return group->name;
}

QString QnUserGroupSettingsModel::getCustomPermissionsDescription(const QnUuid &id, Qn::GlobalPermissions permissions) const
{
    const bool hasAccessToSystem = accessController()->hasGlobalPermission(Qn::GlobalAdminPermission);

    QStringList tableRows;

    auto allResources = qnResPool->getResources();
    auto accessibleResources = qnResourceAccessManager->accessibleResources(id);

    std::map<Filter, QString> nameByFilter{
        {CamerasFilter, tr("Cameras")},
        {LayoutsFilter, tr("Global Layouts")},
        {ServersFilter, tr("Servers")},
    };

    for (auto filter : allFilters())
    {
        auto flag = accessPermission(filter);
        if (permissions.testFlag(flag))
        {
            QString row = lit("<td colspan=2><b>%1</b></td>").arg(accessPermissionText(filter));
            tableRows << kHtmlTableRowTemplate.arg(row);
        }
        else
        {
            auto allFiltered = filteredResources(filter, allResources);
            auto accessibleFiltered = filteredResources(filter, accessibleResources);

            QString row = hasAccessToSystem
                ? lit("<td><b>%1</b> / %2</td><td>%3</td>").arg(accessibleFiltered.size()).arg(allFiltered.size()).arg(nameByFilter[filter])
                : lit("<td><b>%1</b></td><td>%2</td>").arg(accessibleFiltered.size()).arg(nameByFilter[filter]);
            tableRows << kHtmlTableRowTemplate.arg(row);
        }
    }

    return kHtmlTableTemplate.arg(tableRows.join(QString()));
}
