// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>
#include <QtCore/QSet>

#include <core/resource/resource_fwd.h>
#include <nx/core/access/access_types.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/types/access_rights_types.h>
#include <nx/vms/client/desktop/resource_properties/user/utils/access_subject_editing_context.h>
#include <nx/vms/client/desktop/system_context.h>


namespace nx::vms::client::desktop {

class SystemContext;

struct NX_VMS_CLIENT_DESKTOP_API MembersModelGroup
{
    Q_GADGET

    Q_PROPERTY(QnUuid id MEMBER id)
    Q_PROPERTY(QString text MEMBER text)
    Q_PROPERTY(QString description MEMBER description)
    Q_PROPERTY(bool isLdap MEMBER isLdap)
    Q_PROPERTY(bool isPredefined MEMBER isPredefined)

public:
    bool operator==(const MembersModelGroup&) const = default;
    bool operator<(const MembersModelGroup& other) const { return id < other.id; }
    QnUuid id;
    QString text;
    QString description;
    bool isLdap = false;
    bool isPredefined = false;

    static MembersModelGroup fromId(SystemContext* systemContext, const QnUuid& id);
};

class NX_VMS_CLIENT_DESKTOP_API MembersModel: public QAbstractListModel, public SystemContextAware
{
    using base_type = QAbstractListModel;

    Q_OBJECT

    Q_PROPERTY(QnUuid groupId READ groupId WRITE setGroupId NOTIFY groupIdChanged)
    Q_PROPERTY(QnUuid userId READ userId WRITE setUserId NOTIFY userIdChanged)
    Q_PROPERTY(QList<nx::vms::client::desktop::MembersModelGroup> parentGroups
        READ parentGroups
        WRITE setParentGroups
        NOTIFY parentGroupsChanged)
    Q_PROPERTY(QList<QnUuid> groups READ groups WRITE setGroups NOTIFY groupsChanged)
    Q_PROPERTY(QList<QnUuid> users READ users WRITE setUsers NOTIFY usersChanged)
    Q_PROPERTY(nx::core::access::ResourceAccessMap sharedResources
        READ ownSharedResources
        WRITE setOwnSharedResources
        NOTIFY sharedResourcesChanged)
    Q_PROPERTY(int customGroupCount
        READ customGroupCount
        NOTIFY customGroupCountChanged)
    Q_PROPERTY(nx::vms::client::desktop::AccessSubjectEditingContext* editingContext
        READ editingContext
        NOTIFY editingContextChanged)

public:
    enum Roles
    {
        IsUserRole = Qt::UserRole + 1,
        OffsetRole,
        DescriptionRole,
        IsTopLevelRole,
        IsMemberRole,
        IsParentRole,
        IdRole,
        IsAllowedMember,
        IsAllowedParent,
        IsLdap,
        MemberSectionRole, //< IsUserRole ? "U" : "G"
        GroupSectionRole, // Built-in? "B" : "C"
    };

    MembersModel();
    MembersModel(SystemContext* systemContext);

    virtual ~MembersModel();

    virtual QHash<int, QByteArray> roleNames() const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    static void registerQmlType();

    void readUsersAndGroups();

    Q_INVOKABLE QVariant getParentsTree(const QnUuid& groupId) const;

    QList<MembersModelGroup> parentGroups() const;
    void setParentGroups(const QList<MembersModelGroup>& groups);

    QnUuid groupId() const { return m_subjectIsUser ? QnUuid{} : m_subjectId; }
    void setGroupId(const QnUuid& groupId);

    QnUuid userId() const { return m_subjectIsUser ? m_subjectId : QnUuid{}; }
    void setUserId(const QnUuid& userId);

    static bool isPredefined(const QnUuid& groupId);

    void setOwnSharedResources(const nx::core::access::ResourceAccessMap& resources);
    nx::core::access::ResourceAccessMap ownSharedResources() const;

    QList<QnUuid> users() const;
    void setUsers(const QList<QnUuid>& users);

    QList<QnUuid> groups() const;
    void setGroups(const QList<QnUuid>& groups);

    int customGroupCount() const;

    AccessSubjectEditingContext* editingContext() const { return m_subjectContext.get(); }

    // Sort a list of subjects.
    static void sortSubjects(
        QList<QnUuid>& subjects,
        nx::vms::common::SystemContext* systemContext);

signals:
    void groupIdChanged();
    void userIdChanged();
    void parentGroupsChanged();
    void usersChanged();
    void groupsChanged();
    void globalPermissionsChanged();
    void sharedResourcesChanged();
    void editingContextChanged();
    void customGroupCountChanged();

private:
    bool rebuildModel();
    void loadModelData();

    QVariant getGroupData(int offset, const QnUuid& groupId, int role) const;
    QVariant getUserData(int offset, const QnUuid& userId, int role) const;

    std::pair<QnUuid, int> findGroupRow(const QList<QnUuid>& groups, int row) const;
    QVariant getData(int offset, const QnUuid& groupId, int row, int role) const;

    bool isAllowedMember(const QnUuid& groupId) const;
    bool isAllowedParent(const QnUuid& groupId) const;

    void addParent(const QnUuid& groupId);
    void removeParent(const QnUuid& groupId);
    void addMember(const QnUuid& memberId);
    void removeMember(const QnUuid& memberId);

private:
    struct MemberInfo
    {
        QString name;
        QString description;
        bool isGroup = false;
        bool isLdap = false;
    };

    MemberInfo info(const QnUuid& id) const;

    // Get sorted lists of users and groups from a set of subjects.
    std::pair<QList<QnUuid>, QList<QnUuid>> sortedSubjects(const QSet<QnUuid>& subjectSet) const;

    QHash<QnUuid, QSet<QnUuid> > m_sharedResources;
    QScopedPointer<AccessSubjectEditingContext> m_subjectContext;

    int m_itemCount = 0;
    QHash<QnUuid, int> m_totalSubItems;
    QList<QnUuid> m_allGroups;
    QList<QnUuid> m_allUsers;

    QnUuid m_subjectId;
    bool m_subjectIsUser = false;

    int m_customGroupCount = 0;
};

} // namespace nx::vms::client::desktop

Q_DECLARE_METATYPE(nx::vms::client::desktop::MembersModelGroup)
