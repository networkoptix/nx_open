// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource_access/providers/abstract_resource_access_provider.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/api/types/access_rights_types.h>
#include <nx/vms/common/system_context_aware.h>

namespace nx::vms::api { struct UserRoleData; }

namespace nx::core::access {

/**
 * Abstract base class for access providers containing common code.
 * Thread-safety is achieved by using only signal-slot system with auto-connections.
 */
class NX_VMS_COMMON_API BaseResourceAccessProvider:
    public AbstractResourceAccessProvider,
    public nx::vms::common::SystemContextAware
{
    using base_type = AbstractResourceAccessProvider;

public:
    BaseResourceAccessProvider(
        Mode mode,
        nx::vms::common::SystemContext* context,
        QObject* parent = nullptr);
    virtual ~BaseResourceAccessProvider();

    virtual bool hasAccess(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource) const override;

    virtual QSet<QnUuid> accessibleResources(const QnResourceAccessSubject& subject) const override;

    virtual Source accessibleVia(
        const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource,
        QnResourceList* providers = nullptr) const override;

protected:
    virtual void beforeUpdate() override;
    virtual void afterUpdate() override;

    virtual Source baseSource() const = 0;

    virtual bool calculateAccess(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource,
        nx::vms::api::GlobalPermissions globalPermissions) const = 0;

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

    void handleRoleAddedOrUpdated(const nx::vms::api::UserRoleData& userRole);
    void handleRoleRemoved(const nx::vms::api::UserRoleData& userRole);

    virtual void handleSubjectAdded(const QnResourceAccessSubject& subject);
    virtual void handleSubjectRemoved(const QnResourceAccessSubject& subject);

    QSet<QnUuid> accessible(const QnResourceAccessSubject& subject) const;

    bool isLayout(const QnResourcePtr& resource) const;
    bool isMediaResource(const QnResourcePtr& resource) const;

protected:
    mutable nx::Mutex m_mutex;

private:
    // Hash of accessible resources by subject id.
    QHash<QnUuid, QSet<QnUuid> > m_accessibleResources;
};

} // namespace nx::core::access
