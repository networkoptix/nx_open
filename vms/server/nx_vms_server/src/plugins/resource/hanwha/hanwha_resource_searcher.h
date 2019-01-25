#pragma once

#include <plugins/resource/hanwha/hanwha_common.h>

#include <core/resource_management/resource_searcher.h>
#include <plugins/resource/upnp/upnp_resource_searcher.h>
#include <nx/network/upnp/upnp_search_handler.h>
#include <nx/utils/mac_address.h>
#include <core/resource/resource_fwd.h>
#include "hanwha_shared_resource_context.h"
#include <nx/vms/server/server_module_aware.h>

class QnMediaServerModule;

namespace nx {
namespace vms::server {
namespace plugins {

class HanwhaResourceSearcher:
    public QnAbstractNetworkResourceSearcher,
    public nx::network::upnp::SearchAutoHandler,
    public /*mixin*/ nx::vms::server::ServerModuleAware

{
public:
    HanwhaResourceSearcher(QnMediaServerModule* serverModule);
    virtual ~HanwhaResourceSearcher() override = default;

    virtual QnResourcePtr createResource(
        const QnUuid &resourceTypeId,
        const QnResourceParams& params) override;

    virtual QString manufacture() const override;

    virtual QnResourceList findResources() override;

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

    virtual bool isEnabled() const override;

    static QAuthenticator getDefaultAuth();
private:
    void createResource(
        const nx::network::upnp::DeviceInfo& devInfo,
        const nx::utils::MacAddress& mac,
        QnResourceList& result);

    bool isHanwhaCamera(const nx::network::upnp::DeviceInfo& devInfo) const;

    template<typename T>
    void addMultichannelResources(QList<T>& result, const QAuthenticator& auth);
    HanwhaResult<HanwhaInformation> cachedDeviceInfo(const QAuthenticator& auth, const nx::utils::Url& url);
    void addResourcesViaSunApi(QnResourceList& upnpResults);
    void sendSunApiProbe();
    void readSunApiResponse(QnResourceList& resultResourceList);
    bool readSunApiResponseFromSocket(
        nx::network::AbstractDatagramSocket* socket,
        QnResourceList* outResultResourceList);
    void updateSocketList();

    struct SunApiData: public nx::network::upnp::DeviceInfo
    {
        SunApiData() { timer.restart(); }
        nx::utils::MacAddress macAddress;
        QElapsedTimer timer;
    };
    bool parseSunApiData(const QByteArray& data, SunApiData* outData);
    bool isHostBelongsToValidSubnet(const QHostAddress& address) const;
    static std::vector<std::vector<quint8>> createProbePackets();
private:
    struct SessionKeyData
    {
        QString sessionKey;
        QnMutex lock;
    };

    struct BaseDeviceInfo
    {
        int numberOfChannels = 0;
        nx::core::resource::DeviceType deviceType = nx::core::resource::DeviceType::unknown;

        bool isValid() const
        {
            return numberOfChannels != 0
                && deviceType != nx::core::resource::DeviceType::unknown;
        };
    };

    using SessionKeyPtr = std::shared_ptr<SessionKeyData>;

    mutable QnMutex m_mutex;
    QnResourceList m_foundUpnpResources;
    std::set<QString> m_alreadyFoundMacAddresses;
    std::map<QString, BaseDeviceInfo> m_baseDeviceInfos;

    // TODO: #dmishin make different session keys for different session types
    // There is only one session key per group now.

    mutable QMap<QString, SessionKeyPtr> m_sessionKeys;

    const std::vector<std::vector<quint8>> m_sunapiProbePackets;
    std::vector<std::unique_ptr<nx::network::AbstractDatagramSocket>> m_sunApiSocketList;
    std::unique_ptr<nx::network::AbstractDatagramSocket> m_sunapiReceiveSocket;
    QList<nx::network::QnInterfaceAndAddr> m_lastInterfaceList;
    QMap<nx::utils::MacAddress, SunApiData> m_sunapiDiscoveredDevices;
};

} // namespace plugins
} // namespace vms::server
} // namespace nx
