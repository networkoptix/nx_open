#pragma once

#if defined(ENABLE_HANWHA)

#include <core/resource_management/resource_searcher.h>
#include <plugins/resource/upnp/upnp_resource_searcher.h>
#include <nx/network/upnp/upnp_search_handler.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {


class HanwhaResourceSearcher:
	public QnAbstractNetworkResourceSearcher,
	public nx_upnp::SearchHandler
{

public:
    HanwhaResourceSearcher(QnCommonModule* commonModule);

    virtual QnResourcePtr createResource(
        const QnUuid &resourceTypeId,
        const QnResourceParams& params) override;

    // return the manufacture of the server
    virtual QString manufacture() const;

    virtual QnResourceList findResources(void) override;

    virtual QList<QnResourcePtr> checkHostAddr(
        const QUrl& url,
        const QAuthenticator& auth,
        bool doMultichannelCheck) override;

    //Upnp resource searcher
    virtual bool processPacket(
        const QHostAddress& discoveryAddr,
        const SocketAddress& deviceEndpoint,
        const nx_upnp::DeviceInfo& devInfo,
        const QByteArray& xmlDevInfo) override;

private:

    void createResource(
        const nx_upnp::DeviceInfo& devInfo,
        const QnMacAddress& mac,
        const QAuthenticator& auth,
        QnResourceList& result );

    bool isHanwhaCamera(const nx_upnp::DeviceInfo& devInfo) const;
    int getChannels(const HanwhaResourcePtr& resource);

    template<typename T>
    void addMultichannelResources(QList<T>& result);
private:
    QnResourceList m_foundUpnpResources;
    std::set<QString> m_alreadFoundMacAddresses;
    mutable QnMutex m_mutex;
    QMap<QString, int> m_channelsByCamera;
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
