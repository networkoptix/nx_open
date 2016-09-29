#include "resource_access_provider.h"

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

int QnResourceAccessProvider::providersCount() const
{
    return m_providers.size();
}

void QnResourceAccessProvider::handleBaseProviderAccessChanged(
    const QnResourceAccessSubject& subject, const QnResourcePtr& resource, Source value)
{
    auto source = qobject_cast<QnAbstractResourceAccessProvider*>(sender());

    /* Check if access was granted */
    if (value != Source::none)
    {

    }
}

