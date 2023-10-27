// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>

#include <nx/vms/client/desktop/resource_properties/user/utils/access_subject_editing_context.h>
#include <nx/vms/client/desktop/system_context.h>

#include "members_cache.h"

namespace nx::vms::client::desktop {

/**
 * Readonly model that represents recursive members of a specified group.
 */
class NX_VMS_CLIENT_DESKTOP_API RecursiveMembersModel:
    public QAbstractListModel,
    public SystemContextAware
{
    using base_type = QAbstractListModel;

    Q_OBJECT

    Q_PROPERTY(QnUuid groupId READ groupId WRITE setGroupId NOTIFY groupIdChanged)
    Q_PROPERTY(MembersCache* membersCache
        READ membersCache
        WRITE setMembersCache
        NOTIFY membersCacheChanged)
    Q_PROPERTY(bool hasCycle READ hasCycle NOTIFY hasCycleChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles
    {
        IsUserRole = Qt::UserRole + 1,
        OffsetRole,
        IdRole,
        IsLdap,
        IsTemporary,
        CanEditParents,
        UserType
    };

public:
    RecursiveMembersModel();
    RecursiveMembersModel(SystemContext* systemContext);
    virtual ~RecursiveMembersModel();

    virtual QHash<int, QByteArray> roleNames() const override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    static void registerQmlType();

    QnUuid groupId() const { return m_groupId; }
    void setGroupId(const QnUuid& groupId);

    MembersCache* membersCache() const { return m_cache; }
    void setMembersCache(MembersCache* cache);

    bool hasCycle() const { return m_hasCycle; }
    int count() const { return m_itemCount; }

signals:
    void groupIdChanged();
    void membersCacheChanged();
    void hasCycleChanged();
    void countChanged();

private:

    std::pair<QnUuid, int> findGroupRow(const QList<QnUuid>& groups, int row) const;

    QVariant getMemberData(int offset, const QnUuid& groupId, int role) const;

    QVariant getData(int offset, const QnUuid& groupId, int row, int role) const;

    void loadData(bool updateAllRows = true);

private:
    int m_itemCount = 0;
    QHash<QnUuid, int> m_totalSubItems;
    bool m_hasCycle = false;

    QnUuid m_groupId;

    QPointer<MembersCache> m_cache;
};

} // namespace nx::vms::client::desktop
