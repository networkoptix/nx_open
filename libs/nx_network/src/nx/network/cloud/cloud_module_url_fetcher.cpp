// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_module_url_fetcher.h"

#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/software_version.h>

namespace nx::network::cloud {

CloudModuleUrlFetcher::CloudModuleUrlFetcher(const std::string& moduleName):
    m_moduleAttrName(nameset().findResourceByName(moduleName).id)
{
    NX_ASSERT(
        m_moduleAttrName != nx::utils::stree::INVALID_RES_ID,
        nx::format("Given bad cloud module name %1").arg(moduleName));
}

void CloudModuleUrlFetcher::setUrl(nx::utils::Url url)
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    m_url = std::move(url);
}

void CloudModuleUrlFetcher::get(Handler handler)
{
    get(nx::network::http::AuthInfo(), ssl::kDefaultCertificateCheck, std::move(handler));
}

void CloudModuleUrlFetcher::get(
    http::AuthInfo auth, ssl::AdapterFunc proxyAdapterFunc, Handler handler)
{
    using namespace std::chrono;
    using namespace std::placeholders;

    //if requested endpoint is known, providing it to the output
    NX_MUTEX_LOCKER lk(&m_mutex);
    if (m_url)
    {
        auto result = *m_url;
        lk.unlock();
        handler(nx::network::http::StatusCode::ok, std::move(result));
        return;
    }

    initiateModulesXmlRequestIfNeeded(
        std::move(auth), std::move(proxyAdapterFunc), std::move(handler));
}

bool CloudModuleUrlFetcher::analyzeXmlSearchResult(
    const nx::utils::stree::ResourceContainer& searchResult)
{
    std::string foundEndpointStr;
    if (!searchResult.get(m_moduleAttrName, &foundEndpointStr))
        return false;

    m_url = buildUrl(foundEndpointStr, m_moduleAttrName);
    return true;
}

void CloudModuleUrlFetcher::invokeHandler(
    const Handler& handler,
    nx::network::http::StatusCode::Value statusCode)
{
    NX_ASSERT(statusCode != nx::network::http::StatusCode::ok || static_cast<bool>(m_url));
    handler(statusCode, m_url ? *m_url : nx::utils::Url());
}

//-------------------------------------------------------------------------------------------------

CloudModuleUrlFetcher::ScopedOperation::ScopedOperation(
    CloudModuleUrlFetcher* const fetcher)
    :
    m_fetcher(fetcher)
{
}

CloudModuleUrlFetcher::ScopedOperation::~ScopedOperation()
{
}

void CloudModuleUrlFetcher::ScopedOperation::get(Handler handler)
{
    get(nx::network::http::AuthInfo(), ssl::kDefaultCertificateCheck, std::move(handler));
}

void CloudModuleUrlFetcher::ScopedOperation::get(
    nx::network::http::AuthInfo auth, nx::network::ssl::AdapterFunc adapterFunc, Handler handler)
{
    auto sharedGuard = m_guard.sharedGuard();
    m_fetcher->get(auth, std::move(adapterFunc),
        [sharedGuard = std::move(sharedGuard), handler = std::move(handler)](
            nx::network::http::StatusCode::Value statusCode,
            nx::utils::Url result) mutable
        {
            if (auto lock = sharedGuard->lock())
                handler(statusCode, result);
        });
}

//-------------------------------------------------------------------------------------------------
// class CloudDbUrlFetcher

CloudDbUrlFetcher::CloudDbUrlFetcher():
    CloudModuleUrlFetcher(kCloudDbModuleName)
{
    setModulesXmlUrl(
        AppInfo::defaultCloudModulesXmlUrl(
            nx::network::SocketGlobals::cloud().cloudHost()));
}

} // namespace nx::network::cloud
