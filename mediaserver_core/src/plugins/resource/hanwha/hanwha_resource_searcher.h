#pragma once

#if defined(ENABLE_HANWHA)

#include <plugins/resource/hanwha/hanwha_common.h>

#include <core/resource_management/resource_searcher.h>
#include <plugins/resource/upnp/upnp_resource_searcher.h>
#include <nx/network/upnp/upnp_search_handler.h>
#include <core/resource/resource_fwd.h>
#include "hanwha_shared_resource_context.h"

namespace nx {
namespace mediaserver_core {
namespace plugins {


class HanwhaResourceSearcher:
	public QnAbstractNetworkResourceSearcher,
    public nx::network::upnp::SearchAutoHandler
{
public:
    HanwhaResourceSearcher(QnCommonModule* commonModule);
    virtual ~HanwhaResourceSearcher() = default;

    virtual QnResourcePtr createResource(
        const QnUuid &resourceTypeId,
        const QnResourceParams& params) override;

    // return the manufacture of the server
    virtual QString manufacture() const override;

    virtual QnResourceList findResources(void) override;

    virtual QList<QnResourcePtr> checkHostAddr(
        const nx::utils::Url& url,
        const QAuthenticator& auth,
        bool doMultichannelCheck) override;

    //Upnp resource searcher
    virtual bool processPacket(
        const QHostAddress& discoveryAddr,
        const nx::network::SocketAddress& deviceEndpoint,
        const nx::network::upnp::DeviceInfo& devInfo,
        const QByteArray& xmlDevInfo) override;

    static QAuthenticator getDefaultAuth();
private:
    void createResource(
        const nx::network::upnp::DeviceInfo& devInfo,
        const nx::network::QnMacAddress& mac,
        QnResourceList& result );

    bool isHanwhaCamera(const nx::network::upnp::DeviceInfo& devInfo) const;

    template<typename T>
    void addMultichannelResources(QList<T>& result, const QAuthenticator& auth);
    HanwhaResult<HanwhaInformation> cachedDeviceInfo(const QAuthenticator& auth, const nx::utils::Url& url);
    void addResourcesViaSunApi(QnResourceList& upnpResults);
    void sendSunApiProbe();
    void readSunApiResponse(QnResourceList& resultResourceList);
    void updateSocketList();

    struct SunApiData: public nx::network::upnp::DeviceInfo
    {
        SunApiData() { timer.restart(); }
        nx::network::QnMacAddress macAddress;
        QElapsedTimer timer;
    };
    bool parseSunApiData(const QByteArray& data, SunApiData* outData);
    bool isHostBelongsToValidSubnet(const QHostAddress& address) const;
    static std::vector<std::vector<quint8>> createProbePackets();
private:
    QMap<QString, std::shared_ptr<HanwhaSharedResourceContext>> m_sharedContext;
    struct SessionKeyData
    {
        QString sessionKey;
        QnMutex lock;
    };
    using SessionKeyPtr = std::shared_ptr<SessionKeyData>;

    mutable QnMutex m_mutex;
    QnResourceList m_foundUpnpResources;
    std::set<QString> m_alreadFoundMacAddresses;
    QMap<QString, int> m_channelsByCamera;

    // TODO: #dmishin make different session keys for different session types
    // There is only one session key per group now.

    mutable QMap<QString, SessionKeyPtr> m_sessionKeys;

    const std::vector<std::vector<quint8>> m_sunapiProbePackets;
    std::vector<std::unique_ptr<nx::network::AbstractDatagramSocket>> m_sunApiSocketList;
    QList<nx::network::QnInterfaceAndAddr> m_lastInterfaceList;
    QMap<nx::network::QnMacAddress, SunApiData> m_sunapiDiscoveredDevices;
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
