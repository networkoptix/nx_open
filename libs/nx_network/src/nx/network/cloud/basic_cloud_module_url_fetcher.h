#pragma once

#include <vector>

#include <QtCore/QBuffer>
#include <QtCore/QUrl>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/app_info.h>
#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/network/stun/stun_types.h>
#include <nx/utils/app_info.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/std/optional.h>
#include <nx/utils/stree/node.h>
#include <nx/utils/stree/resourcenameset.h>
#include <nx/utils/stree/resourcecontainer.h>
#include <nx/utils/stree/stree_manager.h>
#include <nx/utils/stree/streesaxhandler.h>
#include <nx/utils/thread/mutex.h>

#include "cloud_modules_xml_sax_handler.h"

namespace nx {
namespace network {
namespace cloud {

NX_NETWORK_API extern const char* const kCloudDbModuleName;
NX_NETWORK_API extern const char* const kConnectionMediatorModuleName;
NX_NETWORK_API extern const char* const kConnectionMediatorTcpUrlName;
NX_NETWORK_API extern const char* const kConnectionMediatorUdpUrlName;
NX_NETWORK_API extern const char* const kNotificationModuleName;
NX_NETWORK_API extern const char* const kSpeedTestModuleName;

static constexpr std::chrono::seconds kHttpRequestTimeout = std::chrono::seconds(10);

class CloudInstanceSelectionAttributeNameset:
    public nx::utils::stree::ResourceNameSet
{
public:
    enum AttributeType
    {
        cloudInstanceName = 1,
        vmsVersionMajor,
        vmsVersionMinor,
        vmsVersionBugfix,
        vmsVersionBuild,
        vmsVersionFull,
        vmsBeta,
        vmsCustomization,
        cdbUrl,
        hpmUrl,
        hpmTcpUrl,
        hpmUdpUrl,
        notificationModuleUrl,
        speedTestModuleUrl,
    };

    CloudInstanceSelectionAttributeNameset();
};

//-------------------------------------------------------------------------------------------------

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
        nx::String name, nx::String value)
    {
        m_additionalHttpHeadersForGetRequest.emplace_back(name, value);
    }

protected:
    mutable QnMutex m_mutex;

    virtual bool analyzeXmlSearchResult(
        const nx::utils::stree::ResourceContainer& searchResult) = 0;
    virtual void invokeHandler(
        const Handler& handler,
        nx::network::http::StatusCode::Value statusCode) = 0;

    const CloudInstanceSelectionAttributeNameset& nameset() const
    {
        return m_nameset;
    }

    void initiateModulesXmlRequestIfNeeded(
        const nx::network::http::AuthInfo& auth,
        Handler handler)
    {
        using namespace std::chrono;
        using namespace std::placeholders;

        if (!m_modulesXmlUrl)
        {
            post(
                [this, handler = std::move(handler)]()
                { invokeHandler(handler, http::StatusCode::badRequest); });
            return;
        }

        // If async resolve is already started, should wait for its completion.
        m_resolveHandlers.emplace_back(std::move(handler));

        if (m_requestIsRunning)
            return;

        NX_ASSERT(!m_httpClient);
        // If requested url is unknown, fetching description xml.
        m_httpClient = nx::network::http::AsyncHttpClient::create();
        m_httpClient->setAuth(auth);
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
            std::bind(&BasicCloudModuleUrlFetcher::onHttpClientDone, this, _1));
    }

    nx::utils::Url buildUrl(const QString& str, int moduleAttrName)
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
            const auto it = m_moduleToDefaultUrlScheme.find(
                nameset().findResourceByID(moduleAttrName).name);

            if (it != m_moduleToDefaultUrlScheme.end())
                url.setScheme(it->second);
            else
                url.setScheme(QLatin1String(nx::network::http::kUrlSchemeName));

            if ((url.scheme() == QLatin1String(nx::network::http::kUrlSchemeName))
                && (url.port() == nx::network::http::DEFAULT_HTTPS_PORT))
            {
                url.setScheme(QLatin1String(nx::network::http::kSecureUrlSchemeName));
            }
        }

        return url;
    }

private:
    std::optional<nx::utils::Url> m_modulesXmlUrl;
    nx::network::http::AsyncHttpClientPtr m_httpClient;
    const CloudInstanceSelectionAttributeNameset m_nameset;
    std::vector<Handler> m_resolveHandlers;
    bool m_requestIsRunning;
    std::list<std::pair<nx::String, nx::String>> m_additionalHttpHeadersForGetRequest;
    std::map<QString, QString> m_moduleToDefaultUrlScheme;

    void onHttpClientDone(nx::network::http::AsyncHttpClientPtr client)
    {
        NX_ASSERT(isInSelfAioThread());

        nx::network::http::StatusCode::Value resultCode =
            nx::network::http::StatusCode::ok;
        // Invoking handlers with mutex not locked.
        auto scope = nx::utils::makeScopeGuard(
            [this, &resultCode]() { signalWaitingHandlers(resultCode); });

        QnMutexLocker lk(&m_mutex);

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

        QByteArray xmlData = client->fetchMessageBodyBuffer();
        QBuffer xmlDataSource(&xmlData);
        std::unique_ptr<nx::utils::stree::AbstractNode> stree =
            nx::utils::stree::StreeManager::loadStree(
                &xmlDataSource,
                m_nameset,
                nx::utils::stree::ParseFlag::ignoreUnknownResources);
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
        nx::utils::stree::ResourceContainer inputData;
        const nx::utils::SoftwareVersion productVersion(nx::utils::AppInfo::applicationVersion());
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
            nx::utils::AppInfo::applicationVersion());
        inputData.put(
            CloudInstanceSelectionAttributeNameset::vmsBeta,
            QString::fromLatin1(nx::utils::AppInfo::beta() ? "true" : "false"));
        inputData.put(
            CloudInstanceSelectionAttributeNameset::vmsCustomization,
            nx::utils::AppInfo::customizationName());

        nx::utils::stree::ResourceContainer outputData;
        treeRoot.get(
            nx::utils::stree::MultiSourceResourceReader(inputData, outputData),
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

} // namespace cloud
} // namespace network
} // namespace nx
