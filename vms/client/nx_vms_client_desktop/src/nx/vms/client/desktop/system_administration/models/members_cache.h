// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>

#include <nx/vms/client/desktop/resource_properties/user/utils/access_subject_editing_context.h>
#include <nx/vms/client/desktop/system_administration/globals/user_settings_global.h>
#include <nx/vms/common/system_context.h>

namespace nx::vms::client::desktop {

/**
 * Lazily generated sorted lists of members for each group. Lists are sorted according to UI spec.
 * This class emits signals when the cache is modified, but itself is not subscribed to any signals.
 */
class NX_VMS_CLIENT_DESKTOP_API MembersCache: public QObject
{
    Q_OBJECT

public:
    // Sorted lists of members. It's important that these containers support implicit sharing.
    struct Members
    {
        QList<nx::Uuid> users;
        QList<nx::Uuid> groups;
    };

    // Cached information for each member, which is used for sorting members.
    struct Info
    {
        QString name;
        QString description;
        bool isGroup = false;
        api::UserType userType = api::UserType::local;
    };

    struct Stats
    {
        int localGroups = 0;
        int cloudGroups = 0;
        int ldapGroups = 0;
    };

public:
    MembersCache();
    virtual ~MembersCache();

    Info info(const nx::Uuid& id) const;
    Members sorted(const nx::Uuid& groupId = {}) const;
    void loadInfo(nx::vms::common::SystemContext* systemContext);

    void setContext(AccessSubjectEditingContext* context) { m_subjectContext = context; }

    void sortSubjects(QList<nx::Uuid>& subjects) const;

    int indexIn(const QList<nx::Uuid>& list, const nx::Uuid& id) const;

    Stats stats() const { return m_stats; }

    // Returns indexes of all temporary users.
    QList<int> temporaryUserIndexes() const;

    QList<nx::Uuid> groupsWithTemporary() const { return m_tmpUsersInGroup.keys(); }

public slots:
    void modify(
        const QSet<nx::Uuid>& added,
        const QSet<nx::Uuid>& removed,
        const QSet<nx::Uuid>& groupsWithChangedMembers,
        const QSet<nx::Uuid>& subjectsWithChangedParents);

signals:
    void beginInsert(int at, const nx::Uuid& id, const nx::Uuid& parentId);
    void endInsert(int at, const nx::Uuid& id, const nx::Uuid& parentId);

    void beginRemove(int at, const nx::Uuid& id, const nx::Uuid& parentId);
    void endRemove(int at, const nx::Uuid& id, const nx::Uuid& parentId);

    void reset();

    void statsChanged();

private:
    static MembersCache::Info infoFromContext(
        nx::vms::common::SystemContext* systemContext,
        const nx::Uuid& id);

    // Get sorted lists of users and groups from a set of subjects.
    Members sortedSubjects(const QSet<nx::Uuid>& subjectSet) const;

    std::function<bool(const nx::Uuid&, const nx::Uuid&)> lessFunc() const;

    void updateStats(const QSet<nx::Uuid>& added, const QSet<nx::Uuid>& removed);

    void addTmpUser(const nx::Uuid& groupId, const nx::Uuid& userId);
    void removeTmpUser(const nx::Uuid& groupId, const nx::Uuid& userId);

private:
    mutable QHash<nx::Uuid, Members> m_sortedCache;
    QHash<nx::Uuid, Info> m_info;
    Stats m_stats;
    QHash<nx::Uuid, int> m_tmpUsersInGroup; //< Number of temporary user in each group that has them.
    QSet<nx::Uuid> m_tmpUsers; //< All temporary users.
    AccessSubjectEditingContext* m_subjectContext = nullptr;
    nx::vms::common::SystemContext* m_systemContext = nullptr;
};

} // namespace nx::vms::client::desktop
