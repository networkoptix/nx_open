#include "analytics_metadata_provider_factory.h"

#include <nx/utils/log/assert.h>

namespace nx {
namespace client {
namespace core {

AbstractAnalyticsMetadataProviderFactory::~AbstractAnalyticsMetadataProviderFactory()
{
}

bool AnalyticsMetadataProviderFactory::supportsAnalytics(const QnResourcePtr& resource) const
{
    return !analyticsTypeForResource(resource).isEmpty();
}

AbstractAnalyticsMetadataProviderPtr AnalyticsMetadataProviderFactory::createMetadataProvider(
    const QnResourcePtr& resource) const
{
    const auto& analyticsType = analyticsTypeForResource(resource);
    if (analyticsType.isEmpty())
        return AbstractAnalyticsMetadataProviderPtr();

    const auto it = std::find_if(m_factories.cbegin(), m_factories.cend(),
        [&analyticsType](const Factory& factory)
        {
            return factory.first == analyticsType;
        });

    if (it == m_factories.cend())
        return AbstractAnalyticsMetadataProviderPtr();

    return it->second->createMetadataProvider(resource);
}

void AnalyticsMetadataProviderFactory::registerMetadataFactory(
    const QString& type, AbstractAnalyticsMetadataProviderFactory* factory)
{
    NX_ASSERT(!type.isEmpty(), "Analytics type cannot be empty.");
    m_factories.append(
        Factory{
            type,
            QSharedPointer<AbstractAnalyticsMetadataProviderFactory>(factory)});
}

QString AnalyticsMetadataProviderFactory::analyticsTypeForResource(
    const QnResourcePtr& resource) const
{
    const auto it = std::find_if(m_factories.cbegin(), m_factories.cend(),
        [&resource](const Factory& factory)
        {
            return factory.second->supportsAnalytics(resource);
        });

    return (it != m_factories.cend()) ? it->first : QString();
}

} // namespace core
} // namespace client
} // namespace nx
