#pragma once

#include <nx/network/http/buffer_source.h>
#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>

namespace nx::network::cloud::test {

/**
 * Http test server for cloud modules xml.
 * example xml served:
 *  <sequence>
 *      <set resName="cdb" resValue="https://nxvms.com:443"/>
 *      <set resName="hpm.tcpUrl" resValue="http://mediator.vmsproxy.com:80/mediator"/>
 *      <set resName="hpm.udpUrl" resValue="stun://mediator.vmsproxy.com:3345"/>
 *      <set resName="notification_module" resValue="https://nxvms.com:443"/>
 *      <set resName="speedtest_module" resValue="http://speedtest.vmsproxy.com"/>
 *  </sequence>
 */
class NX_NETWORK_API CloudModulesXmlServer
{
public:
    void registerRequestHandlers(
        const std::string& basePath,
        network::http::server::rest::MessageDispatcher* messageDispatcher);

    /**
     * @return a string containing {basePath}/cloud_modules.xml, or nothing if registerRequestHandlers
     * hasn't been called.
     */
    std::optional<std::string> cloudModulesXmlPath() const;

    void setCdbUrl(const nx::utils::Url& url);
    void setHpmTcpUrl(const nx::utils::Url& url);
    void setHpmUdpUrl(const nx::utils::Url& url);
    void setNotificationModuleUrl(const nx::utils::Url& url);
    void setSpeedTestUrl(const nx::utils::Url& url);

private:
    void setModule(const char* resName, const nx::utils::Url& resValue);
    QByteArray serializeModules() const;

    void serve(
        network::http::RequestContext requestContext,
        network::http::RequestProcessedHandler handler);

private:
    std::optional<std::string> m_cloudModulesXmlPath;
    mutable QnMutex m_mutex;
    std::map<std::string /*resName*/, nx::utils::Url /*resValue*/> m_modules;
};

} // namespace nx::network::cloud::test