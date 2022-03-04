// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource_access/providers/abstract_resource_access_provider.h>
#include <nx/vms/common/resource/resource_context_aware.h>

namespace nx::core::access {

/**
 * Class-facade that collects data from base providers. Governs ways of access to resources.
 * For example, camera may be accessible directly - or by shared layout where it is located.
 */
class NX_VMS_COMMON_API ResourceAccessProvider:
    public AbstractResourceAccessProvider,
    public nx::vms::common::ResourceContextAware
{
    using base_type = AbstractResourceAccessProvider;

public:
    ResourceAccessProvider(
        Mode mode,
        nx::vms::common::ResourceContext* context,
        QObject* parent = nullptr);
    virtual ~ResourceAccessProvider();

    virtual bool hasAccess(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource) const override;

    virtual QSet<QnUuid> accessibleResources(const QnResourceAccessSubject& subject) const override;

    virtual Source accessibleVia(
        const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource,
        QnResourceList* providers = nullptr) const override;

    /**
     * Get all access ways to the given resource.
     */
    QSet<Source> accessLevels(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource) const;

    /**
     * Add new base provider to the end of the list.
     * NOTICE: this class will take ownership!
     */
    void addBaseProvider(AbstractResourceAccessProvider* provider);

    /**
     * Add new base provider to the specified place.
     * NOTICE: this class will take ownership!
     */
    void insertBaseProvider(int index, AbstractResourceAccessProvider* provider);

    /**
     * Remove base provider from the module.
     * NOTICE: this class will release ownership! You must remove the provider yourself.
     */
    void removeBaseProvider(AbstractResourceAccessProvider* provider);

    QList<AbstractResourceAccessProvider*> providers() const;

    virtual void clear() override;

protected:
    virtual void beginUpdateInternal() override;
    virtual void endUpdateInternal() override;
    virtual void afterUpdate() override;

private:
    void handleBaseProviderAccessChanged(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource, Source value);

private:
    QList<AbstractResourceAccessProvider*> m_providers;
};

template<typename ResourceType>
QnSharedResourcePointerList<ResourceType> getAccessibleResources(
    const QnUserResourcePtr& subject,
    QnSharedResourcePointerList<ResourceType> resources,
    nx::core::access::ResourceAccessProvider* accessProvider)
{
    const auto itEnd = std::remove_if(resources.begin(), resources.end(),
        [accessProvider, subject](const QnSharedResourcePointer<ResourceType>& other)
        {
            return !accessProvider->hasAccess(subject, other);
        });
    resources.erase(itEnd, resources.end());
    return resources;
}

} // namespace nx::core::access
