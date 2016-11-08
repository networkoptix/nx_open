#pragma once

#include <core/resource_access/resource_access_subject.h>

#include <nx/utils/singleton.h>
#include <nx/utils/thread/mutex.h>

#include <utils/common/connective.h>

class QnResourceAccessSubjectsCache:
    public Connective<QObject>,
    public Singleton<QnResourceAccessSubjectsCache>
{
    Q_OBJECT
    typedef Connective<QObject> base_type;

public:
    QnResourceAccessSubjectsCache(QObject* parent = nullptr);

    /** List of all subjects of the resources access: users and roles. */
    QList<QnResourceAccessSubject> allSubjects() const;

    /** List of users, belonging to given role. */
    QList<QnResourceAccessSubject> dependentSubjects(const QnResourceAccessSubject& subject) const;

private:
    void handleUserAdded(const QnUserResourcePtr& user);
    void handleUserRemoved(const QnUserResourcePtr& user);
    void updateUserDependency(const QnUserResourcePtr& user);

    void handleRoleAdded(const ec2::ApiUserGroupData& userRole);
    void handleRoleRemoved(const ec2::ApiUserGroupData& userRole);
private:
    mutable QnMutex m_mutex;

    QList<QnResourceAccessSubject> m_subjects;
    QHash<QnUuid, QnUuid> m_roleByUser;
    QHash<QnUuid, QList<QnResourceAccessSubject>> m_dependent;
};

#define qnResourceAccessSubjectsCache QnResourceAccessSubjectsCache::instance()
