// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <QtCore/QBuffer>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/network/stun/stun_types.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/std/optional.h>
#include <nx/utils/stree/node.h>
#include <nx/utils/stree/attribute_dictionary.h>
#include <nx/utils/stree/stree_manager.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/url.h>

#include <nx/branding.h>
#include <nx/build_info.h>

namespace nx::network::cloud {

static constexpr char kCloudDbModuleName[] = "cdb";
static constexpr char kConnectionMediatorModuleName[] = "hpm";
static constexpr char kConnectionMediatorTcpUrlName[] = "hpm.tcpUrl";
static constexpr char kConnectionMediatorUdpUrlName[] = "hpm.udpUrl";
static constexpr char kNotificationModuleName[] = "notification_module";
static constexpr char kSpeedTestModuleName[] = "speedtest_module";

namespace CloudInstanceSelectionAttributeNameset {

static constexpr char cloudInstanceName[] = "cloud.instance.name";
static constexpr char vmsVersionMajor[] = "vms.version.major";
static constexpr char vmsVersionMinor[] = "vms.version.minor";
static constexpr char vmsVersionBugfix[] = "vms.version.bugfix";
static constexpr char vmsVersionBuild[] = "vms.version.build";
static constexpr char vmsVersionFull[] = "vms.version.full";
static constexpr char vmsCustomization[] = "vms.customization";
static constexpr auto cdbUrl = kCloudDbModuleName;
static constexpr auto hpmUrl = kConnectionMediatorModuleName;
static constexpr auto hpmTcpUrl = kConnectionMediatorTcpUrlName;
static constexpr auto hpmUdpUrl = kConnectionMediatorUdpUrlName;
static constexpr auto notificationModuleUrl = kNotificationModuleName;
static constexpr auto speedTestModuleUrl = kSpeedTestModuleName;

} // namespace CloudInstanceSelectionAttributeNameset

//-------------------------------------------------------------------------------------------------

static constexpr std::chrono::seconds kHttpRequestTimeout{10};

/**
 * Looks up online API url of a specified cloud module.
 */
template<typename Handler>
class BasicCloudModuleUrlFetcher:
    public aio::BasicPollable
{
    using base_type = aio::BasicPollable;

public:
    BasicCloudModuleUrlFetcher():
        m_requestIsRunning(false)
    {
        // Preparing compatibility data.
        m_moduleToDefaultUrlScheme.emplace(kCloudDbModuleName, nx::network::http::kUrlSchemeName);
        m_moduleToDefaultUrlScheme.emplace(kConnectionMediatorModuleName, nx::network::stun::kUrlSchemeName);
        m_moduleToDefaultUrlScheme.emplace(kNotificationModuleName, nx::network::http::kUrlSchemeName);
    }

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override
    {
        base_type::bindToAioThread(aioThread);

        if (m_httpClient)
            m_httpClient->bindToAioThread(aioThread);
    }

    virtual void stopWhileInAioThread() override
    {
        base_type::stopWhileInAioThread();

        // We do not need mutex here since no one uses object anymore
        //    and internal events are delivered in same aio thread.
        m_httpClient.reset();
    }

    void setModulesXmlUrl(nx::utils::Url url)
    {
        m_modulesXmlUrl = std::move(url);
    }

    void addAdditionalHttpHeaderForGetRequest(
        std::string name, std::string value)
    {
        m_additionalHttpHeadersForGetRequest.emplace_back(name, value);
    }

protected:
    mutable nx::Mutex m_mutex;

    virtual bool analyzeXmlSearchResult(
        const nx::utils::stree::AttributeDictionary& searchResult) = 0;

    virtual void invokeHandler(
        const Handler& handler,
        nx::network::http::StatusCode::Value statusCode) = 0;

    void initiateModulesXmlRequestIfNeeded(
        const nx::network::http::AuthInfo& auth,
        nx::network::ssl::AdapterFunc proxyAdapterFunc,
        Handler handler)
    {
        using namespace std::chrono;
        using namespace std::placeholders;

        if (!m_modulesXmlUrl)
        {
            post([this, handler = std::move(handler)]() {
                invokeHandler(handler, http::StatusCode::badRequest);
            });
            return;
        }

        // If async resolve is already started, should wait for its completion.
        m_resolveHandlers.emplace_back(std::move(handler));

        if (m_requestIsRunning)
            return;

        NX_ASSERT(!m_httpClient);
        // If requested url is unknown, fetching description xml.
        m_httpClient = nx::network::http::AsyncHttpClient::create(ssl::kDefaultCertificateCheck);
        m_httpClient->setCredentials(auth.credentials);
        if (!auth.proxyEndpoint.isNull())
        {
            m_httpClient->setProxyCredentials(auth.proxyCredentials);
            m_httpClient->setProxyVia(
                auth.proxyEndpoint, auth.isProxySecure, std::move(proxyAdapterFunc));
        }
        m_httpClient->bindToAioThread(getAioThread());

        for (const auto& header: m_additionalHttpHeadersForGetRequest)
            m_httpClient->addAdditionalHeader(header.first, header.second);

        m_httpClient->setSendTimeoutMs(
            duration_cast<milliseconds>(kHttpRequestTimeout).count());
        m_httpClient->setResponseReadTimeoutMs(
            duration_cast<milliseconds>(kHttpRequestTimeout).count());
        m_httpClient->setMessageBodyReadTimeoutMs(
            duration_cast<milliseconds>(kHttpRequestTimeout).count());

        m_requestIsRunning = true;

        m_httpClient->doGet(
            *m_modulesXmlUrl,
            [this](auto&&... args) { onHttpClientDone(std::forward<decltype(args)>(args)...); });
    }

    nx::utils::Url buildUrl(const std::string& str, const std::string& moduleAttrName)
    {
        nx::utils::Url url(str);
        if (url.host().isEmpty())
        {
            // str could be host:port.
            const SocketAddress endpoint(str);
            url = nx::utils::Url();
            url.setHost(endpoint.address.toString());
            if (endpoint.port > 0)
                url.setPort(endpoint.port);
        }

        if (url.scheme().isEmpty())
        {
            const auto it = m_moduleToDefaultUrlScheme.find(moduleAttrName);

            if (it != m_moduleToDefaultUrlScheme.end())
                url.setScheme(it->second);
            else
                url.setScheme(nx::network::http::kUrlSchemeName);

            if ((url.scheme() == nx::network::http::kUrlSchemeName)
                && (url.port() == nx::network::http::DEFAULT_HTTPS_PORT))
            {
                url.setScheme(nx::network::http::kSecureUrlSchemeName);
            }
        }

        return url;
    }

private:
    std::optional<nx::utils::Url> m_modulesXmlUrl;
    nx::network::http::AsyncHttpClientPtr m_httpClient;
    std::vector<Handler> m_resolveHandlers;
    bool m_requestIsRunning;
    std::list<std::pair<std::string, std::string>> m_additionalHttpHeadersForGetRequest;
    std::map<std::string, std::string> m_moduleToDefaultUrlScheme;

    void onHttpClientDone(nx::network::http::AsyncHttpClientPtr client)
    {
        NX_ASSERT(isInSelfAioThread());

        nx::network::http::StatusCode::Value resultCode =
            nx::network::http::StatusCode::ok;
        // Invoking handlers with mutex not locked.
        auto scope = nx::utils::makeScopeGuard(
            [this, &resultCode]() { signalWaitingHandlers(resultCode); });

        NX_MUTEX_LOCKER lk(&m_mutex);

        m_httpClient.reset();

        if (!client->response())
        {
            resultCode = nx::network::http::StatusCode::serviceUnavailable;
            return;
        }

        if (client->response()->statusLine.statusCode != nx::network::http::StatusCode::ok)
        {
            resultCode = static_cast<nx::network::http::StatusCode::Value>(
                client->response()->statusLine.statusCode);
            return;
        }

        QByteArray xmlData = toByteArray(client->fetchMessageBodyBuffer());
        std::unique_ptr<nx::utils::stree::AbstractNode> stree =
            nx::utils::stree::StreeManager::loadStree(xmlData);
        if (!stree)
        {
            resultCode = nx::network::http::StatusCode::serviceUnavailable;
            return;
        }

        // Selecting endpoint.
        if (!findModuleUrl(*stree))
        {
            resultCode = nx::network::http::StatusCode::notFound;
            return;
        }

        resultCode = nx::network::http::StatusCode::ok;
    }

    bool findModuleUrl(const nx::utils::stree::AbstractNode& treeRoot)
    {
        nx::utils::stree::AttributeDictionary inputData;
        const nx::utils::SoftwareVersion productVersion(nx::build_info::vmsVersion());
        inputData.put(
            CloudInstanceSelectionAttributeNameset::vmsVersionMajor,
            productVersion.major());
        inputData.put(
            CloudInstanceSelectionAttributeNameset::vmsVersionMinor,
            productVersion.minor());
        inputData.put(
            CloudInstanceSelectionAttributeNameset::vmsVersionBugfix,
            productVersion.bugfix());
        inputData.put(
            CloudInstanceSelectionAttributeNameset::vmsVersionBuild,
            productVersion.build());
        inputData.put(
            CloudInstanceSelectionAttributeNameset::vmsVersionFull,
            nx::build_info::vmsVersion());
        inputData.put(
            CloudInstanceSelectionAttributeNameset::vmsCustomization,
            nx::branding::customization());

        nx::utils::stree::AttributeDictionary outputData;
        treeRoot.get(
            nx::utils::stree::makeMultiReader(inputData, outputData),
            &outputData);

        return analyzeXmlSearchResult(outputData);
    }

    void signalWaitingHandlers(nx::network::http::StatusCode::Value statusCode)
    {
        decltype(m_resolveHandlers) handlers;
        m_resolveHandlers.swap(handlers);
        m_requestIsRunning = false;
        for (auto& handler: handlers)
            invokeHandler(handler, statusCode);
    }
};

} // namespace nx::network::cloud
