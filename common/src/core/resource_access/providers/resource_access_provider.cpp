#include "resource_access_provider.h"

#include <nx/utils/log/assert.h>

QnResourceAccessProvider::QnResourceAccessProvider(QObject* parent):
    base_type(parent)
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
    const QnResourceAccessSubject& subject, const QnResourcePtr& resource) const
{
    for (auto provider : m_providers)
    {
        auto result = provider->accessibleVia(subject, resource);
        if (result != Source::none)
            return result;
    }

    return Source::none;
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

void QnResourceAccessProvider::handleBaseProviderAccessChanged(
    const QnResourceAccessSubject& subject, const QnResourcePtr& resource, Source value)
{
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

