// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <unordered_map>

#include <QtCore/QAbstractListModel>
#include <QtCore/QSet>

#include <core/resource/resource_fwd.h>
#include <nx/utils/scoped_connections.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/types/access_rights_types.h>
#include <nx/vms/client/desktop/resource_properties/user/utils/access_subject_editing_context.h>
#include <nx/vms/client/desktop/system_context.h>

#include "members_cache.h"

namespace nx::vms::client::desktop {

class SystemContext;

/* Wrapper for a QList<nx::Uuid> to enable implicit sharing when passing through QML. */
struct NX_VMS_CLIENT_DESKTOP_API MembersListWrapper
{
    Q_GADGET

    Q_PROPERTY(qsizetype length READ length)

public:
    MembersListWrapper() {}
    MembersListWrapper(const QList<nx::Uuid>& members): m_members(members) {}

    template<typename Begin, typename End>
    MembersListWrapper(Begin membersBegin, End membersEnd): m_members(membersBegin, membersEnd) {}

    bool operator==(const MembersListWrapper& other) const { return m_members == other.m_members; }
    bool operator==(const QList<nx::Uuid>& other) const { return m_members == other; }
    operator QList<nx::Uuid>() const { return m_members; }

    qsizetype length() const { return m_members.size(); }
    QList<nx::Uuid> list() const { return m_members; }

private:
    QList<nx::Uuid> m_members;
};

struct NX_VMS_CLIENT_DESKTOP_API MembersModelGroup
{
    Q_GADGET

    Q_PROPERTY(nx::Uuid id MEMBER id)
    Q_PROPERTY(QString text MEMBER text)
    Q_PROPERTY(QString description MEMBER description)
    Q_PROPERTY(bool isLdap MEMBER isLdap)
    Q_PROPERTY(bool isPredefined MEMBER isPredefined)

public:
    bool operator==(const MembersModelGroup&) const = default;
    bool operator<(const MembersModelGroup& other) const { return id < other.id; }
    nx::Uuid id;
    QString text;
    QString description;
    bool isLdap = false;
    bool isPredefined = false;

    static MembersModelGroup fromId(SystemContext* systemContext, const nx::Uuid& id);
};

class NX_VMS_CLIENT_DESKTOP_API MembersModel: public QAbstractListModel, public SystemContextAware
{
    using base_type = QAbstractListModel;

    Q_OBJECT

    Q_PROPERTY(nx::Uuid groupId READ groupId WRITE setGroupId NOTIFY groupIdChanged)
    Q_PROPERTY(nx::Uuid userId READ userId WRITE setUserId NOTIFY userIdChanged)
    Q_PROPERTY(bool temporary READ temporary WRITE setTemporary NOTIFY temporaryChanged)
    Q_PROPERTY(QList<nx::vms::client::desktop::MembersModelGroup> parentGroups
        READ parentGroups
        WRITE setParentGroups
        NOTIFY parentGroupsChanged)
    Q_PROPERTY(QList<nx::Uuid> groups READ groups WRITE setGroups NOTIFY groupsChanged)
    Q_PROPERTY(MembersListWrapper users READ users WRITE setUsers NOTIFY usersChanged)
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
    Q_PROPERTY(MembersCache* membersCache READ membersCache NOTIFY membersCacheChanged)
    Q_PROPERTY(bool cycledGroup READ isCycledGroup NOTIFY cycledGroupChanged)

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
        IsTemporary,
        MemberSectionRole, //< IsUserRole ? "U" : "G"
        GroupSectionRole, // Built-in "B", Custom "C" or LDAP "L"
        Cycle,
        CanEditParents,
        CanEditMembers,
        IsPredefined,
        UserType
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

    QList<MembersModelGroup> parentGroups() const;
    void setParentGroups(const QList<MembersModelGroup>& groups);

    nx::Uuid groupId() const { return m_subjectIsUser ? nx::Uuid{} : m_subjectId; }
    void setGroupId(const nx::Uuid& groupId);

    nx::Uuid userId() const { return m_subjectIsUser ? m_subjectId : nx::Uuid{}; }
    void setUserId(const nx::Uuid& userId);

    bool temporary() const { return m_temporary; }
    void setTemporary(bool value);

    void setOwnSharedResources(const nx::core::access::ResourceAccessMap& resources);
    nx::core::access::ResourceAccessMap ownSharedResources() const;

    MembersListWrapper users() const;
    void setUsers(const MembersListWrapper& users);

    QList<nx::Uuid> groups() const;
    void setGroups(const QList<nx::Uuid>& groups);

    int customGroupCount() const;

    AccessSubjectEditingContext* editingContext() const { return m_subjectContext.get(); }

    // Sort a list of subjects.
    void sortSubjects(QList<nx::Uuid>& subjects) const;

    MembersCache* membersCache() const { return m_cache.get(); }

    bool canEditMembers(const nx::Uuid& id) const;

    Q_INVOKABLE void removeParent(const nx::Uuid& groupId);

    bool isCycledGroup() const;

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
    void membersCacheChanged();
    void temporaryChanged();
    void cycledGroupChanged();

private:
    void loadModelData();

    bool isAllowed(const nx::Uuid& parentId, const nx::Uuid& childId) const;

    void addParent(const nx::Uuid& groupId);
    void addMember(const nx::Uuid& memberId);
    void removeMember(const nx::Uuid& memberId);

    void subscribeToUser(const QnUserResourcePtr& user);
    void unsubscribeFromUser(const QnUserResourcePtr& user);

    void checkCycles();

private:
    struct DeleteLater
    {
        void operator()(QObject* o) { o->deleteLater(); }
    };

private:
    nx::utils::ScopedConnections m_connections;
    std::unordered_map<nx::Uuid, nx::utils::ScopedConnections> m_userConnections;

    std::unique_ptr<AccessSubjectEditingContext, DeleteLater> m_subjectContext;

    nx::Uuid m_subjectId;
    bool m_subjectIsUser = false;
    bool m_temporary = false; //< m_subjectId is a temporary user, implies m_subjectIsUser == true.

    MembersCache::Members m_subjectMembers; //< Sorted by id so states can be matched.

    QScopedPointer<MembersCache> m_cache;
    QSet<nx::Uuid> m_groupsWithCycles;
};

} // namespace nx::vms::client::desktop
