// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "parent_groups_provider.h"

#include <QtQml/QtQml>

#include <core/resource_access/subject_hierarchy.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/vms/client/desktop/system_administration/models/members_model.h>
#include <nx/vms/client/desktop/system_context.h>

namespace nx::vms::client::desktop {

ParentGroupsProvider::ParentGroupsProvider(QObject* parent):
    base_type(parent)
{
}

ParentGroupsProvider::~ParentGroupsProvider()
{
}

AccessSubjectEditingContext* ParentGroupsProvider::context() const
{
    return m_context;
}

void ParentGroupsProvider::setContext(AccessSubjectEditingContext* value)
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

QVariant ParentGroupsProvider::data(const QModelIndex& index, int role) const
{
    if (index.row() < 0 || index.row() >= (int) m_groups.size() || !m_context)
        return {};

    switch (role)
    {
        case Qt::DisplayRole:
        {
            const auto groupsManager = m_context->systemContext()->userRolesManager();
            const auto group = groupsManager->userRole(m_groups.at(index.row()));
            return group.name;
        }
        case isLdap:
        {
            const auto groupsManager = m_context->systemContext()->userRolesManager();
            const auto group = groupsManager->userRole(m_groups.at(index.row()));
            return group.isLdap;
        }
        case isParent:
        {
            const auto directParents =
                m_context->subjectHierarchy()->directParents(m_context->currentSubjectId());

            return directParents.contains(m_groups.at(index.row()));
        }
        case isPredefined:
            return MembersModel::isPredefined(m_groups.at(index.row()));

        default:
            return {};
    }
}

bool ParentGroupsProvider::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (index.row() < 0
        || index.row() >= (int) m_groups.size()
        || !m_context
        || role != isParent)
    {
        return false;
    }

    if (!data(index, role).toBool() || value.toBool()) // We can only uncheck.
        return false;

    auto parents = m_context->subjectHierarchy()->directParents(m_context->currentSubjectId());
    auto members = m_context->subjectHierarchy()->directMembers(m_context->currentSubjectId());

    parents.remove(m_groups.at(index.row()));

    m_context->setRelations(parents, members);

    return true;
}

int ParentGroupsProvider::rowCount(const QModelIndex&) const
{
    return m_groups.size();
}

QHash<int, QByteArray> ParentGroupsProvider::roleNames() const
{
    auto names = base_type::roleNames();

    names.insert(isParent, "isParent");
    names.insert(isLdap, "isLdap");
    names.insert(isPredefined, "isPredefined");
    names[Qt::DisplayRole] = "text";

    return names;
}

void ParentGroupsProvider::updateInfo()
{
    QList<QnUuid> groups;

    if (m_context)
    {
        const auto directParents =
            m_context->subjectHierarchy()->directParents(m_context->currentSubjectId());

        groups = m_context->subjectHierarchy()->recursiveParents(
                {m_context->currentSubjectId()}).values();

        MembersModel::sortSubjects(groups, m_context->systemContext());
    }

    const int diff = (int) groups.size() - m_groups.size();

    if (diff > 0)
    {
        beginInsertRows({}, m_groups.size(), m_groups.size() + diff - 1);
        m_groups = groups;
        endInsertRows();
    }
    else if (diff < 0)
    {
        beginRemoveRows({}, m_groups.size() + diff, m_groups.size() - 1);
        m_groups = groups;
        endRemoveRows();
    }
    else
    {
        m_groups = groups;
    }

    if (!groups.empty())
    {
        emit dataChanged(
            this->index(0, 0),
            this->index(m_groups.size() - 1, 0));
    }
}

class ParentGroupsModel: public QSortFilterProxyModel
{
    using base_type = QSortFilterProxyModel;
public:
};

void ParentGroupsProvider::registerQmlTypes()
{
    qmlRegisterType<ParentGroupsProvider>(
        "nx.vms.client.desktop", 1, 0, "ParentGroupsProvider");
    qmlRegisterType<ParentGroupsModel>(
        "nx.vms.client.desktop", 1, 0, "ParentGroupsModel");
}

} // namespace nx::vms::client::desktop
