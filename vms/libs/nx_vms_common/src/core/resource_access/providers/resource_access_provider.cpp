// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_access_provider.h"

#include <core/resource_access/resource_access_subjects_cache.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/common/system_context.h>

namespace nx::core::access {

ResourceAccessProvider::ResourceAccessProvider(
    Mode mode,
    nx::vms::common::SystemContext* context,
    QObject* parent)
    :
    base_type(mode, parent),
    nx::vms::common::SystemContextAware(context)
{
}

ResourceAccessProvider::~ResourceAccessProvider()
{
}

bool ResourceAccessProvider::hasAccess(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource) const
{
    return std::any_of(m_providers.cbegin(), m_providers.cend(),
        [&subject, &resource](AbstractResourceAccessProvider* provider)
        {
            return provider->hasAccess(subject, resource);
        });
}

QSet<QnUuid> ResourceAccessProvider::accessibleResources(
    const QnResourceAccessSubject& subject) const
{
    QSet<QnUuid> result;
    for (const auto& provider: m_providers)
        result.unite(provider->accessibleResources(subject));
    return result;
}

Source ResourceAccessProvider::accessibleVia(
    const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource,
    QnResourceList* providers) const
{
    if (providers)
        providers->clear();

    Source accessSource = Source::none;
    for (const auto& provider: m_providers)
    {
        const auto result = provider->accessibleVia(subject, resource, providers);
        if (accessSource == Source::none)
            accessSource = result;

        // If we need provider resources, we must check all child providers.
        if (!providers && result != Source::none)
            break;
    }

    // Make sure if there is at least one provider then access is granted.
    if (providers && !providers->isEmpty())
    {
        NX_ASSERT(accessSource != Source::none);
    }

    return accessSource;
}

QSet<Source> ResourceAccessProvider::accessLevels(
    const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource) const
{
    QSet<Source> result;
    for (const auto& provider: m_providers)
    {
        const auto level = provider->accessibleVia(subject, resource);
        if (level != Source::none)
            result.insert(level);
    }
    return result;
}

void ResourceAccessProvider::addBaseProvider(AbstractResourceAccessProvider* provider)
{
    provider->setParent(this);
    m_providers.append(provider);

    if (mode() == Mode::cached)
    {
        connect(provider, &AbstractResourceAccessProvider::accessChanged, this,
            &ResourceAccessProvider::handleBaseProviderAccessChanged, Qt::DirectConnection);
    }
}

void ResourceAccessProvider::insertBaseProvider(int index,
    AbstractResourceAccessProvider* provider)
{
    provider->setParent(this);
    m_providers.insert(index, provider);

    if (mode() == Mode::cached)
    {
        connect(provider, &AbstractResourceAccessProvider::accessChanged, this,
            &ResourceAccessProvider::handleBaseProviderAccessChanged, Qt::DirectConnection);
    }
}

void ResourceAccessProvider::removeBaseProvider(AbstractResourceAccessProvider* provider)
{
    NX_ASSERT(provider);
    if (!provider)
        return;

    NX_ASSERT(m_providers.contains(provider));
    m_providers.removeOne(provider);

    if (provider->parent() == this)
        provider->setParent(nullptr);
}

QList<AbstractResourceAccessProvider*> ResourceAccessProvider::providers() const
{
    return m_providers;
}

void ResourceAccessProvider::beginUpdateInternal()
{
    if (mode() == Mode::direct)
        return;

    for (auto provider: std::as_const(m_providers))
        provider->beginUpdate();
}

void ResourceAccessProvider::endUpdateInternal()
{
    if (mode() == Mode::direct)
        return;

    for (auto provider: std::as_const(m_providers))
        provider->endUpdate();
}

void ResourceAccessProvider::afterUpdate()
{
    if (mode() == Mode::direct)
        return;

    // This check assures client will receive all required notifications if something was modified
    // on the server while it was in the 'Reconnecting' state.
    const auto allSubjects = m_context->resourceAccessSubjectsCache()->allSubjects();
    const auto resources = m_context->resourcePool()->getResources();
    for (const auto& subject: allSubjects)
    {
        for (const QnResourcePtr& resource: resources)
        {
            auto value = accessibleVia(subject, resource);
            if (value != Source::none)
                emit accessChanged(subject, resource, value);
        }
    }
}

void ResourceAccessProvider::handleBaseProviderAccessChanged(
    const QnResourceAccessSubject& subject, const QnResourcePtr& resource, Source value)
{
    NX_ASSERT(mode() == Mode::cached);

    if (isUpdating())
        return;

    auto source = qobject_cast<AbstractResourceAccessProvider*>(sender());

    auto sourceIt = std::find(m_providers.cbegin(), m_providers.cend(), source);
    NX_ASSERT(sourceIt != m_providers.cend());

    // If we already have access from more important provider just ignore signal.
    for (auto it = m_providers.cbegin(); it != sourceIt; ++it)
    {
        const auto provider = *it;
        if (provider->accessibleVia(subject, resource) != Source::none)
            return;
    }

    // Check if access was granted. Just notify.
    if (value != Source::none)
    {
        emit accessChanged(subject, resource, value);
        return;
    }

    if (sourceIt != m_providers.cend())
        ++sourceIt;

    // Access was removed. Check if we still have access through less important provider.
    for (auto it = sourceIt; it != m_providers.cend(); ++it)
    {
        auto provider = *it;
        auto result = provider->accessibleVia(subject, resource);
        if (result != Source::none)
        {
            emit accessChanged(subject, resource, result);
            return;
        }
    }

    // No access left at all.
    emit accessChanged(subject, resource, value);
}

void ResourceAccessProvider::clear()
{
    for (AbstractResourceAccessProvider* provider : m_providers)
        provider->clear();
}

} // namespace nx::core::access
