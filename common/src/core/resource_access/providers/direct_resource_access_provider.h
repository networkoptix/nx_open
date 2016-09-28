#pragma once

#include <core/resource_access/providers/abstract_resource_access_provider.h>

/** Handles access via 'shared resources' db table. */
class QnDirectResourceAccessProvider: public QnAbstractResourceAccessProvider
{
    using base_type = QnAbstractResourceAccessProvider;
public:
    QnDirectResourceAccessProvider(QObject* parent = nullptr);
    virtual ~QnDirectResourceAccessProvider();

    virtual bool hasAccess(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource) const override;

private:
    bool acceptable(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource) const;

    bool calculateAccess(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource) const;
    void updateAccess(const QnResourceAccessSubject& subject, const QnResourcePtr& resource);

    void handleResourceAdded(const QnResourcePtr& resource);
    void handleResourceRemoved(const QnResourcePtr& resource);

    void handleRoleAddedOrUpdated(const ec2::ApiUserGroupData& userRole);
    void handleRoleRemoved(const ec2::ApiUserGroupData& userRole);

    void handleAccessibleResourcesChanged(const QnResourceAccessSubject& subject,
        const QSet<QnUuid>& resourceIds);
private:
    /** Hash of accessible resources by subject effective id. */
    QHash<QnUuid, QSet<QnUuid> > m_accessibleResources;
};
