#pragma once

#include <core/resource_access/providers/abstract_resource_access_provider.h>

/** Abstract base class for access providers containing common code.
 *  Thread-safety is achieved by using only signal-slot system with auto-connections.
 */
class QnBaseResourceAccessProvider: public QnAbstractResourceAccessProvider
{
    using base_type = QnAbstractResourceAccessProvider;
public:
    QnBaseResourceAccessProvider(QObject* parent = nullptr);
    virtual ~QnBaseResourceAccessProvider();

    virtual bool hasAccess(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource) const override;

    virtual Source accessibleVia(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource) const override;

protected:
    virtual Source baseSource() const = 0;

    virtual bool calculateAccess(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource) const = 0;

    bool acceptable(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource) const;

    void updateAccessToResource(const QnResourcePtr& resource);
    void updateAccess(const QnResourceAccessSubject& subject, const QnResourcePtr& resource);
    virtual void handleResourceAdded(const QnResourcePtr& resource);
    virtual void handleResourceRemoved(const QnResourcePtr& resource);

    void handleRoleAddedOrUpdated(const ec2::ApiUserGroupData& userRole);
    void handleRoleRemoved(const ec2::ApiUserGroupData& userRole);

    QSet<QnUuid> accessible(const QnResourceAccessSubject& subject) const;

    bool isLayout(const QnResourcePtr& resource) const;
    bool isMediaResource(const QnResourcePtr& resource) const;

private:
    /** Hash of accessible resources by subject id. */
    QHash<QnUuid, QSet<QnUuid> > m_accessibleResources;

};
