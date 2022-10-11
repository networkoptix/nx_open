// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_searcher.h"

#include <core/resource/resource_type.h>
#include <api/global_settings.h>
#include <common/common_module.h>
#include <core/resource/resource.h>
#include <nx/network/http/http_types.h>
#include <nx/network/http/http_client.h>

QnAbstractResourceSearcher::QnAbstractResourceSearcher(QnCommonModule* commonModule) noexcept:
    QnCommonModuleAware(commonModule),
    m_discoveryMode(DiscoveryMode::fullyEnabled),
    m_isLocal(false),
    m_shouldStop(false)
{
}

QnResourceList QnAbstractResourceSearcher::search()
{
    auto result = findResources();
    for (const auto& resource: result)
        resource->setCommonModule(commonModule());
    return result;
}

void QnAbstractResourceSearcher::setDiscoveryMode(DiscoveryMode mode) noexcept
{
    m_discoveryMode = mode;
}

DiscoveryMode QnAbstractResourceSearcher::discoveryMode() const noexcept
{
    const auto& settings = commonModule()->globalSettings();
    if (settings->isInitialized() && settings->isNewSystem())
        return DiscoveryMode::disabled;
    return m_discoveryMode;
}

void QnAbstractResourceSearcher::pleaseStop()
{
    m_shouldStop = true;
}

bool QnAbstractResourceSearcher::shouldStop() const noexcept
{
    return m_shouldStop;
}

bool QnAbstractResourceSearcher::isLocal() const noexcept
{
    return m_isLocal;
}

void QnAbstractResourceSearcher::setLocal(bool l) noexcept
{
    m_isLocal = l;
}

bool QnAbstractResourceSearcher::isResourceTypeSupported(QnUuid resourceTypeId) const
{
    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);
    if (resourceType.isNull())
        return false;

    return resourceType->getManufacturer() == manufacturer();
}

QnAbstractNetworkResourceSearcher::QnAbstractNetworkResourceSearcher(QnCommonModule* commonModule):
    QnAbstractResourceSearcher(commonModule)
{
}

nx::utils::Url QnAbstractNetworkResourceSearcher::updateUrlToHttpsIfNeed(const nx::utils::Url& foundUrl)
{
    if (foundUrl.scheme() == nx::network::http::kSecureUrlSchemeName)
        return foundUrl;

    nx::utils::Url requestUrl(foundUrl);
    if (foundUrl.port() == -1)
        requestUrl.setScheme(nx::network::http::kSecureUrlSchemeName);

    nx::network::http::HttpClient client{nx::network::ssl::kAcceptAnyCertificate};
    if (!client.doGet(requestUrl) || !client.response())
        return foundUrl;

    return client.contentLocationUrl();
}
