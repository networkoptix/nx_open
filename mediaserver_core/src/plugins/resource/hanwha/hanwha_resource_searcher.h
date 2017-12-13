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
    public nx_upnp::SearchAutoHandler
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

    static QAuthenticator getDefaultAuth();
private:
    void createResource(
        const nx_upnp::DeviceInfo& devInfo,
        const QnMacAddress& mac,
        QnResourceList& result );

    bool isHanwhaCamera(const nx_upnp::DeviceInfo& devInfo) const;

    template<typename T>
    void addMultichannelResources(QList<T>& result, const QAuthenticator& auth);
    HanwhaResult<HanwhaInformation> cachedDeviceInfo(const QAuthenticator& auth, const QUrl& url);
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
    //mutable QMap<QString, QString> m_sessionKeys;
    //mutable QMap<QString, bool> m_groupLocks;
    //mutable QnWaitCondition m_wait;
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
