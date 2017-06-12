#pragma once

#include <core/resource_access/providers/abstract_resource_access_provider.h>

#include <nx/utils/thread/mutex.h>
#include <common/common_module_aware.h>

/** Abstract base class for access providers containing common code.
 *  Thread-safety is achieved by using only signal-slot system with auto-connections.
 */
class QnBaseResourceAccessProvider: public QnAbstractResourceAccessProvider, public QnCommonModuleAware
{
    using base_type = QnAbstractResourceAccessProvider;

public:
    QnBaseResourceAccessProvider(QObject* parent = nullptr);
    virtual ~QnBaseResourceAccessProvider();

    virtual bool hasAccess(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource) const override;

    virtual Source accessibleVia(
        const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource,
        QnResourceList* providers = nullptr) const override;

protected:
    virtual void beforeUpdate() override;
    virtual void afterUpdate() override;

    virtual Source baseSource() const = 0;

    virtual bool calculateAccess(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource) const = 0;

    bool acceptable(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource) const;

    bool isSubjectEnabled(const QnResourceAccessSubject& subject) const;

    void updateAccessToResource(const QnResourcePtr& resource);
    void updateAccessBySubject(const QnResourceAccessSubject& subject);
    void updateAccess(const QnResourceAccessSubject& subject, const QnResourcePtr& resource);

    virtual void fillProviders(
        const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource,
        QnResourceList& providers) const;

    virtual void handleResourceAdded(const QnResourcePtr& resource);
    virtual void handleResourceRemoved(const QnResourcePtr& resource);

    void handleRoleAddedOrUpdated(const ec2::ApiUserRoleData& userRole);
    void handleRoleRemoved(const ec2::ApiUserRoleData& userRole);

    virtual void handleSubjectAdded(const QnResourceAccessSubject& subject);
    virtual void handleSubjectRemoved(const QnResourceAccessSubject& subject);

    QSet<QnUuid> accessible(const QnResourceAccessSubject& subject) const;

    bool isLayout(const QnResourcePtr& resource) const;
    bool isMediaResource(const QnResourcePtr& resource) const;

protected:
    mutable QnMutex m_mutex;

private:
    /** Hash of accessible resources by subject id. */
    QHash<QnUuid, QSet<QnUuid> > m_accessibleResources;
};
