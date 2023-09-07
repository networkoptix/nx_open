// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "parent_groups_provider.h"

#include <QtQml/QtQml>

#include <core/resource_access/subject_hierarchy.h>
#include <nx/vms/client/desktop/system_administration/models/members_cache.h>
#include <nx/vms/client/desktop/system_administration/models/members_model.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/user_management/user_group_manager.h>

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

void ParentGroupsProvider::setMembersModel(nx::vms::client::desktop::MembersModel* model)
{
    if (model == m_membersModel)
        return;

    if (m_membersModel)
        m_membersModel->disconnect(this);

    m_membersModel = model;
    emit membersModelChanged();

    const auto updateModel =
        [this]()
        {
            const auto count = rowCount();

            if (count > 0)
                emit dataChanged(index(0), index(count - 1), {MembersModel::Roles::CanEditMembers});
        };

    connect(m_membersModel, &QAbstractItemModel::modelReset, this, updateModel);
    connect(m_membersModel, &QAbstractItemModel::rowsInserted, this, updateModel);
    connect(m_membersModel, &QAbstractItemModel::rowsRemoved, this, updateModel);
}

QVariant ParentGroupsProvider::data(const QModelIndex& index, int role) const
{
    if (index.row() < 0 || index.row() >= (int) m_groups.size() || !m_context)
        return {};

    const auto id = m_groups.at(index.row());

    const auto group =
        [this, id]()
        {
            return m_context->systemContext()->userGroupManager()->find(id)
                .value_or(api::UserGroupData{});
        };

    switch (role)
    {
        case Qt::DisplayRole:
            return group().name;

        case Qt::ToolTipRole:
        {
            const auto currentGroup = group();

            if (currentGroup.type == nx::vms::api::UserType::ldap)
                return tr("LDAP group membership is managed in LDAP");

            QStringList names;
            for (const QnUuid& id: currentGroup.parentGroupIds)
            {
                const auto group = m_context->systemContext()->userGroupManager()->find(id);
                if (group)
                    names.append(group->name);
            }

            if (!names.isEmpty())
                return tr("Inherited from %1").arg("<b>" + names.join("</b>, <b>") + "</b>");

            return {};
        }

        case MembersModel::IsLdap:
            return group().type == nx::vms::api::UserType::ldap;

        case MembersModel::IsParentRole:
        {
            const auto directParents =
                m_context->subjectHierarchy()->directParents(m_context->currentSubjectId());

            return directParents.contains(id);
        }

        case MembersModel::IsPredefined:
            return MembersCache::isPredefined(id);

        case MembersModel::CanEditMembers:
            return m_membersModel->canEditMembers(id);

        case MembersModel::IdRole:
            return QVariant::fromValue(id);

        default:
            return {};
    }
}

bool ParentGroupsProvider::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (index.row() < 0
        || index.row() >= (int) m_groups.size()
        || !m_context
        || role != MembersModel::IsParentRole)
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

    names.insert(MembersModel::IsParentRole, "isParent");
    names.insert(MembersModel::IsLdap, "isLdap");
    names.insert(MembersModel::IsPredefined, "isPredefined");
    names.insert(MembersModel::CanEditMembers, "canEditMembers");
    names.insert(MembersModel::IdRole, "id");
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

        if (!groups.empty() && NX_ASSERT(m_membersModel) && m_membersModel->membersCache())
            m_membersModel->membersCache()->sortSubjects(groups);
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

ParentGroupsModel::ParentGroupsModel(QObject* object): base_type(object)
{
    sort(0); //< Enable sorting.
}

bool ParentGroupsModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    if (!sourceModel() || !m_directOnly)
        return base_type::filterAcceptsRow(sourceRow, sourceParent);

    const auto sourceIndex = sourceModel()->index(sourceRow, 0);
    return sourceModel()->data(sourceIndex, MembersModel::IsParentRole).toBool()
        && base_type::filterAcceptsRow(sourceRow, sourceParent);
}

void ParentGroupsModel::setDirectOnly(bool value)
{
    if (value == m_directOnly)
        return;

    m_directOnly = value;
    emit directOnlyChanged();

    invalidateRowsFilter();
}

bool ParentGroupsModel::lessThan(
    const QModelIndex& sourceLeft,
    const QModelIndex& sourceRight) const
{
    const bool leftParent = sourceLeft.data(MembersModel::IsParentRole).toBool();
    const bool rightParent = sourceRight.data(MembersModel::IsParentRole).toBool();

    // Direct parents go in front of non-direct parents.
    if (leftParent != rightParent)
        return leftParent;

    // Otherwise maintain source model sorting.
    return sourceLeft.row() < sourceRight.row();
}

void ParentGroupsProvider::registerQmlTypes()
{
    qmlRegisterType<ParentGroupsProvider>(
        "nx.vms.client.desktop", 1, 0, "ParentGroupsProvider");
    qmlRegisterType<ParentGroupsModel>(
        "nx.vms.client.desktop", 1, 0, "ParentGroupsModel");
}

} // namespace nx::vms::client::desktop
