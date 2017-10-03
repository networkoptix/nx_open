#pragma once

#if defined(ENABLE_HANWHA)

#include <plugins/resource/hanwha/hanwha_common.h>

#include <core/resource_management/resource_searcher.h>
#include <plugins/resource/upnp/upnp_resource_searcher.h>
#include <nx/network/upnp/upnp_search_handler.h>
#include <core/resource/resource_fwd.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {


class HanwhaResourceSearcher:
	public QnAbstractNetworkResourceSearcher,
	public nx_upnp::SearchHandler
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
        const QUrl& url,
        const QAuthenticator& auth,
        bool doMultichannelCheck) override;

    //Upnp resource searcher
    virtual bool processPacket(
        const QHostAddress& discoveryAddr,
        const SocketAddress& deviceEndpoint,
        const nx_upnp::DeviceInfo& devInfo,
        const QByteArray& xmlDevInfo) override;

    QString sessionKey(
        const HanwhaResourcePtr resource,
        HanwhaSessionType sessionType,
        bool generateNewOne = false) const;

private:
    void createResource(
        const nx_upnp::DeviceInfo& devInfo,
        const QnMacAddress& mac,
        const QAuthenticator& auth,
        QnResourceList& result );

    bool isHanwhaCamera(const nx_upnp::DeviceInfo& devInfo) const;
    int getChannels(const HanwhaResourcePtr& resource, const QAuthenticator& auth);

    template<typename T>
    void addMultichannelResources(QList<T>& result, const QAuthenticator& auth);
private:

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
    //mutable QMap<QString, QString> m_sessionKeys;
    //mutable QMap<QString, bool> m_groupLocks;
    //mutable QnWaitCondition m_wait;
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
