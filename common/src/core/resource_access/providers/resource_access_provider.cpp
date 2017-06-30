#include "resource_access_provider.h"

#include <core/resource_access/resource_access_subjects_cache.h>

#include <core/resource_management/resource_pool.h>

#include <nx/utils/log/assert.h>
#include <common/common_module.h>

QnResourceAccessProvider::QnResourceAccessProvider(QObject* parent):
    base_type(parent),
    QnCommonModuleAware(parent)
{
}

QnResourceAccessProvider::~QnResourceAccessProvider()
{
}

bool QnResourceAccessProvider::hasAccess(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource) const
{
    return std::any_of(m_providers.cbegin(), m_providers.cend(),
        [subject, resource](QnAbstractResourceAccessProvider* provider)
        {
            return provider->hasAccess(subject, resource);
        });
}

QnAbstractResourceAccessProvider::Source QnResourceAccessProvider::accessibleVia(
    const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource,
    QnResourceList* providers) const
{
    if (providers)
        providers->clear();

    QnAbstractResourceAccessProvider::Source accessSource = Source::none;
    for (auto provider: m_providers)
    {
        const auto result = provider->accessibleVia(subject, resource, providers);
        if (accessSource == Source::none)
            accessSource = result;

        /* If we need provider resources, we must check all child providers. */
        if (!providers && result != Source::none)
            break;
    }

    /* Make sure if there is at least one provider then access is granted. */
    if (providers && !providers->isEmpty())
    {
        NX_ASSERT(accessSource != Source::none);
    }

    return accessSource;
}

QSet<QnAbstractResourceAccessProvider::Source> QnResourceAccessProvider::accessLevels(
    const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource) const
{
    QSet<QnAbstractResourceAccessProvider::Source> result;
    for (auto provider : m_providers)
    {
        const auto level = provider->accessibleVia(subject, resource);
        if (level != Source::none)
            result << level;
    }
    return result;
}

void QnResourceAccessProvider::addBaseProvider(QnAbstractResourceAccessProvider* provider)
{
    provider->setParent(this);
    m_providers.append(provider);

    connect(provider, &QnAbstractResourceAccessProvider::accessChanged, this,
        &QnResourceAccessProvider::handleBaseProviderAccessChanged);
}

void QnResourceAccessProvider::insertBaseProvider(int index,
    QnAbstractResourceAccessProvider* provider)
{
    provider->setParent(this);
    m_providers.insert(index, provider);
    connect(provider, &QnAbstractResourceAccessProvider::accessChanged, this,
        &QnResourceAccessProvider::handleBaseProviderAccessChanged);
}

void QnResourceAccessProvider::removeBaseProvider(QnAbstractResourceAccessProvider* provider)
{
    NX_ASSERT(provider);
    if (!provider)
        return;

    NX_ASSERT(m_providers.contains(provider));
    m_providers.removeOne(provider);

    if (provider->parent() == this)
        provider->setParent(nullptr);
}

QList<QnAbstractResourceAccessProvider*> QnResourceAccessProvider::providers() const
{
    return m_providers;
}

void QnResourceAccessProvider::beginUpdateInternal()
{
    for (auto p: m_providers)
        p->beginUpdate();
}

void QnResourceAccessProvider::endUpdateInternal()
{
    for (auto p: m_providers)
        p->endUpdate();
}

void QnResourceAccessProvider::afterUpdate()
{
    for (const auto& subject: resourceAccessSubjectsCache()->allSubjects())
    {
        for (const QnResourcePtr& resource: commonModule()->resourcePool()->getResources())
        {
            auto value = accessibleVia(subject, resource);
            if (value != QnAbstractResourceAccessProvider::Source::none)
                emit accessChanged(subject, resource, value);
        }
    }
}

void QnResourceAccessProvider::handleBaseProviderAccessChanged(
    const QnResourceAccessSubject& subject, const QnResourcePtr& resource, Source value)
{
    if (isUpdating())
        return;

    auto source = qobject_cast<QnAbstractResourceAccessProvider*>(sender());

    auto sourceIt = std::find(m_providers.cbegin(), m_providers.cend(), source);
    NX_ASSERT(sourceIt != m_providers.cend());

    /* If we already have access from more important provider just ignore signal. */
    for (auto it = m_providers.cbegin(); it != sourceIt; ++it)
    {
        auto provider = *it;
        if (provider->accessibleVia(subject, resource) != Source::none)
            return;
    }

    /* Check if access was granted. Just notify. */
    if (value != Source::none)
    {
        emit accessChanged(subject, resource, value);
        return;
    }

    if (sourceIt != m_providers.cend())
        ++sourceIt;

    /* Access was removed. Check if we still have access through less important provider. */
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

    /* No access left at all. */
    emit accessChanged(subject, resource, value);
}

