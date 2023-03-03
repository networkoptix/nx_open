// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource_access/resource_access_subject.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/access_rights_data.h>
#include <nx/vms/common/system_context_aware.h>

/** Manager class for shared resources: layouts, cameras, web pages and server statistics. */
class NX_VMS_COMMON_API QnSharedResourcesManager:
    public QObject,
    public nx::vms::common::SystemContextAware
{
    Q_OBJECT

    using base_type = QObject;
public:
    QnSharedResourcesManager(nx::vms::common::SystemContext* context, QObject* parent = nullptr);
    virtual ~QnSharedResourcesManager();

    void reset(const nx::vms::api::AccessRightsDataList& accessibleResourcesList);

    /**
     * List of resources ids, the given user has access to.
     * @param precalculatedEffectiveIds Precalculated user roles effective IDs.
     */
    QSet<QnUuid> sharedResources(
        const QnResourceAccessSubject& subject,
        const std::vector<QnUuid>* const precalculatedEffectiveIds = nullptr) const;

    bool hasSharedResource(
        const std::vector<QnUuid>& subjectRolesEffectiveIds, const QnUuid& resourceId) const;
    void setSharedResources(const QnResourceAccessSubject& subject, const QSet<QnUuid>& resources);

    /**
     * Expliticly set shared resources without checks for subject existance and without signals
     * sending. Actual as workaround the situation when saveAccessRights transaction is received
     * before the saveUser / saveRole transaction.
     */
    void setSharedResourcesById(const QnUuid& subjectId, const QSet<QnUuid>& resources);

    void setSharedResourcesInternal(const QnResourceAccessSubject& subject,
        const QSet<QnUuid>& resources);
    QSet<QnUuid> sharedResourcesInternal(const QnResourceAccessSubject& subject) const;

private:
    void handleResourceAdded(const QnResourcePtr& resource);
    void handleResourceRemoved(const QnResourcePtr& resource);

    void handleRoleAddedOrUpdated(const nx::vms::api::UserRoleData& userRole);
    void handleRoleRemoved(const nx::vms::api::UserRoleData& userRole);
    void handleSubjectRemoved(const QnResourceAccessSubject& subject);

signals:
    void sharedResourcesChanged(const QnResourceAccessSubject& subject,
        const QSet<QnUuid>& oldValues, const QSet<QnUuid>& newValues);

private:
    mutable nx::Mutex m_mutex;
    QHash<QnUuid, QSet<QnUuid> > m_sharedResources;
};
