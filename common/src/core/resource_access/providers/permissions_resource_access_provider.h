#pragma once

#include <core/resource_access/providers/abstract_resource_access_provider.h>

/** Handles access via global permissions. */
class QnPermissionsResourceAccessProvider: public QnAbstractResourceAccessProvider
{
    using base_type = QnAbstractResourceAccessProvider;
public:
    QnPermissionsResourceAccessProvider(QObject* parent = nullptr);
    virtual ~QnPermissionsResourceAccessProvider();

    virtual bool hasAccess(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource) const override;

private:
    bool hasAccessToDesktopCamera(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource) const;

    bool acceptable(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource) const;

    bool calculateAccess(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource) const;
    void updateAccess(const QnResourceAccessSubject& subject, const QnResourcePtr& resource);

    void handleResourceAdded(const QnResourcePtr& resource);
    void handleResourceRemoved(const QnResourcePtr& resource);

    void handleRoleAddedOrUpdated(const ec2::ApiUserGroupData& userRole);
    void handleRoleRemoved(const ec2::ApiUserGroupData& userRole);
private:
    /** Hash of accessible resources by subject effective id. */
    QHash<QnUuid, QSet<QnUuid> > m_accessibleResources;
};
