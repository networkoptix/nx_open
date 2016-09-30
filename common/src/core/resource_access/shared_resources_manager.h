#pragma once

#include <core/resource_access/resource_access_subject.h>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/singleton.h>
#include <nx/utils/uuid.h>

/** Manager class for shared resources: layouts, cameras, web pages and server statistics. */
class QnSharedResourcesManager: public QObject, public Singleton<QnSharedResourcesManager>
{
    Q_OBJECT

    using base_type = QObject;
public:
    QnSharedResourcesManager(QObject* parent = nullptr);
    virtual ~QnSharedResourcesManager();

    void reset(const ec2::ApiAccessRightsDataList& accessibleResourcesList);

    /** List of resources ids, the given user has access to (only given directly). */
    QSet<QnUuid> sharedResources(const QnResourceAccessSubject& subject) const;
    void setSharedResources(const QnResourceAccessSubject& subject, const QSet<QnUuid>& resources);

private:
    void setSharedResourcesInternal(const QnResourceAccessSubject& subject,
        const QSet<QnUuid>& resources);

    void handleResourceRemoved(const QnResourcePtr& resource);
    void handleRoleRemoved(const ec2::ApiUserGroupData& userRole);
    void handleSubjectRemoved(const QnResourceAccessSubject& subject);

signals:
    void sharedResourcesChanged(const QnResourceAccessSubject& subject,
        const QSet<QnUuid>& resourceIds);

private:
    mutable QnMutex m_mutex;
    QHash<QnUuid, QSet<QnUuid> > m_sharedResources;
};

#define qnSharedResourcesManager QnSharedResourcesManager::instance()
