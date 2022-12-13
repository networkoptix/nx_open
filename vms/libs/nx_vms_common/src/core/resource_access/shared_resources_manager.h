// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

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
     * List of Resources ids with their access rights, the given User or Role has access to (only
     * given directly).
     */
    std::map<QnUuid, nx::vms::api::AccessRights> sharedResourceRights(
        const QnResourceAccessSubject& subject) const;

    std::map<QnUuid, nx::vms::api::AccessRights> directAccessRights(
        const QnResourceAccessSubject& subject) const;

    std::map<QnUuid, nx::vms::api::AccessRights> inheritedAccessRights(
        const QnResourceAccessSubject& subject) const;

    bool hasSharedResource(const QnResourceAccessSubject& subject, const QnUuid& resourceId) const;

    void setSharedResourceRights(
        const QnResourceAccessSubject& subject,
        const std::map<QnUuid, nx::vms::api::AccessRights>& resourceRights);

    /**
     * Expliticly set shared resources without checks for subject existance and without signals
     * sending. Actual as workaround the situation when saveAccessRights transaction is received
     * before the saveUser / saveRole transaction.
     */
    void setSharedResourceRights(const nx::vms::api::AccessRightsData& accessRights);

    // TODO: Remove this method when only sharedResourceRights will be used.
    QSet<QnUuid> sharedResources(const QnResourceAccessSubject& subject) const;

    // TODO: Remove this method when only setSharedResourceRights will be used.
    void setSharedResources(
        const QnResourceAccessSubject& subject,
        const QSet<QnUuid>& resources,
        nx::vms::api::AccessRights accessRights = nx::vms::api::AccessRight::view);

private:
    void setSharedResourceRightsInternal(
        const QnResourceAccessSubject& subject,
        const std::map<QnUuid, nx::vms::api::AccessRights>& resourceRights);

    void handleResourceAdded(const QnResourcePtr& resource);
    void handleResourceRemoved(const QnResourcePtr& resource);

    void handleRoleAddedOrUpdated(const nx::vms::api::UserRoleData& userRole);
    void handleRoleRemoved(const nx::vms::api::UserRoleData& userRole);
    void handleSubjectRemoved(const QnResourceAccessSubject& subject);

signals:
    void sharedResourcesChanged(
        const QnResourceAccessSubject& subject,
        const std::map<QnUuid, nx::vms::api::AccessRights>& oldValues,
        const std::map<QnUuid, nx::vms::api::AccessRights>& newValues);

private:
    mutable nx::Mutex m_mutex;
    QHash<QnUuid, std::map<QnUuid, nx::vms::api::AccessRights>> m_sharedResources;
};
