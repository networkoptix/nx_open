// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "global_permissions_model.h"

#include <QtCore/QCollator>
#include <QtQml/QtQml>

#include <core/resource/user_resource.h>
#include <core/resource_access/subject_hierarchy.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/api/types/access_rights_types.h>
#include <nx/vms/client/desktop/system_administration/models/members_model.h>
#include <nx/vms/client/desktop/system_administration/models/members_sort.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/user_management/user_group_manager.h>

namespace nx::vms::client::desktop {

namespace {

std::vector<std::pair<api::GlobalPermission, QString>> globalPermissions()
{
    return {
        {api::GlobalPermission::viewLogs, GlobalPermissionsModel::tr("View event log")},
        {api::GlobalPermission::generateEvents, GlobalPermissionsModel::tr("Generate events")},
    };
};

} // namespace

GlobalPermissionsModel::GlobalPermissionsModel(QObject* parent):
    base_type(parent)
{
}

GlobalPermissionsModel::~GlobalPermissionsModel()
{
}

AccessSubjectEditingContext* GlobalPermissionsModel::context() const
{
    return m_context;
}

void GlobalPermissionsModel::setContext(AccessSubjectEditingContext* value)
{
    if (m_context == value)
        return;

    m_contextConnections.reset();
    m_context = value;

    updateInfo();

    if (m_context)
    {
        m_contextConnections <<
            connect(m_context, &AccessSubjectEditingContext::hierarchyChanged, this,
                [this]() { updateInfo(); });
    }

    emit contextChanged();
}

void GlobalPermissionsModel::setOwnGlobalPermissions(nx::vms::api::GlobalPermissions permissions)
{
    if (permissions != m_ownPermissions)
    {
        m_ownPermissions = permissions;
        emit globalPermissionsChanged();
        emit dataChanged(
            this->index(0, 0),
            this->index(globalPermissions().size() - 1, 0));
    }
}

nx::vms::api::GlobalPermissions GlobalPermissionsModel::ownGlobalPermissions() const
{
    return m_ownPermissions;
}

QVariant GlobalPermissionsModel::data(const QModelIndex& index, int role) const
{
    const auto permissions = globalPermissions();
    if (index.row() < 0 || index.row() >= (int) permissions.size())
        return {};

    switch (role)
    {
        case Qt::DisplayRole:
            return permissions.at(index.row()).second;

        case isChecked:
            return m_ownPermissions.testFlag(permissions.at(index.row()).first);

        case Qt::ToolTipRole:
        {
            if (!m_context)
                return {};

            using namespace nx::vms::common;

            const QSet<QnUuid> groupIds =
                m_context->globalPermissionSource(permissions.at(index.row()).first);

            std::vector<nx::vms::api::UserGroupData> groupsData;

            for (const auto& groupId: groupIds)
            {
                if (const auto group = m_context->systemContext()->userGroupManager()->find(groupId))
                    groupsData.push_back(*group);
            }

            QCollator collator;
            collator.setCaseSensitivity(Qt::CaseInsensitive);
            collator.setNumericMode(true);

            std::sort(groupsData.begin(), groupsData.end(),
                [](const auto& left, const auto& right)
                {
                    return ComparableGroup(left) < ComparableGroup(right);
                });

            if (groupsData.empty())
                return {};

            const QString groupsInfoTemplate = groupsData.size() == 1
                ? tr("%1 group", "%1 will be substituted with a user group name")
                : tr("%1 and %n more groups", "%1 will be substituted with a user group name",
                    groupsData.size() - 1);

            const QString permisionName = permissions.at(index.row()).second;

            return tr("Inherits %1 permission from",
                "%1 will be substituted with a permission name").arg(html::bold(permisionName))
                + " " + groupsInfoTemplate.arg(html::bold(groupsData[0].name));
        }

        default:
            return {};
    }
}

bool GlobalPermissionsModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    const auto permissions = globalPermissions();
    if (index.row() < 0
        || index.row() >= (int) permissions.size()
        || !m_context
        || role != isChecked)
    {
        return false;
    }

    const auto permission = permissions.at(index.row()).first;
    const bool current = m_ownPermissions.testFlag(permission);

    if (value.toBool() == current)
        return false;

    m_ownPermissions.setFlag(permission, value.toBool());
    emit globalPermissionsChanged();

    return true;
}

int GlobalPermissionsModel::rowCount(const QModelIndex&) const
{
    return globalPermissions().size();
}

QHash<int, QByteArray> GlobalPermissionsModel::roleNames() const
{
    auto names = base_type::roleNames();

    names.insert(isChecked, "isChecked");

    return names;
}

void GlobalPermissionsModel::updateInfo()
{
    api::GlobalPermissions permissions = {};

    if (m_context)
    {
        const auto resourcePool = m_context->systemContext()->resourcePool();
        const auto groupManager = m_context->systemContext()->userGroupManager();
        const auto id = m_context->currentSubjectId();

        if (const auto group = groupManager->find(id))
            permissions = group->permissions;
        else if (const auto user = resourcePool->getResourceById<QnUserResource>(id))
            permissions = user->getRawPermissions();
    }

    if (permissions != m_ownPermissions)
    {
        m_ownPermissions = permissions;
        emit globalPermissionsChanged();
    }

    emit dataChanged(
        this->index(0, 0),
        this->index(globalPermissions().size() - 1, 0));
}

void GlobalPermissionsModel::registerQmlTypes()
{
    qmlRegisterType<GlobalPermissionsModel>(
        "nx.vms.client.desktop", 1, 0, "GlobalPermissionsModel");
}

} // namespace nx::vms::client::desktop
