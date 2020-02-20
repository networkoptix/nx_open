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
class CloudModulesXmlServer
{
public:
    static constexpr char kRequestPath[] = "/cloud_modules.xml";

    class Modules
    {
        public:
            void setCdbUrl(const nx::utils::Url& url)
            {
                m_modules["cdb"] = url;
            }

            void setHpmTcpUrl(const nx::utils::Url& url)
            {
                m_modules["hpm.tcpUrl"] = url;
            }

            void setHpmUdpUrl(const nx::utils::Url& url)
            {
                m_modules["hpm.udpUrl"] = url;
            }

            void setNotificationModuleUrl(const nx::utils::Url& url)
            {
                m_modules["notification_module"] = url;
            }

            void setSpeedTestUrl(const nx::utils::Url& url)
            {
                m_modules["speedtest_module"] = url;
            }

        private:
            friend class CloudModulesXmlServer;

            std::map<std::string/*resName*/, nx::utils::Url/*resValue*/> m_modules;
    };

public:
    void setModules(Modules modules);

    void registerHandler(
        const std::string& basePath,
        network::http::server::rest::MessageDispatcher* messageDispatcher);

private:
    void serve(
        network::http::RequestContext requestContext,
        network::http::RequestProcessedHandler handler);

    static QByteArray toXml(const Modules& modules);

private:
    Modules m_modules;
};

} // namespace nx::network::cloud::test