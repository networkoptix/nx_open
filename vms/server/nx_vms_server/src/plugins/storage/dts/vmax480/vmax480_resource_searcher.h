#pragma once

#ifdef ENABLE_VMAX

#include "plugins/resource/upnp/upnp_resource_searcher.h"

class CLSimpleHTTPClient;
class QnMediaServerModule;

class QnPlVmax480ResourceSearcher : public QnUpnpResourceSearcherAsync
{
public:

    QnPlVmax480ResourceSearcher(QnMediaServerModule* serverModule);
    ~QnPlVmax480ResourceSearcher();

    virtual QnResourcePtr createResource(
        const QnUuid &resourceTypeId, const QnResourceParams& params) override;

    virtual QString manufacturer() const override;

    virtual QList<QnResourcePtr> checkHostAddr(
        const nx::utils::Url& url, const QAuthenticator& auth, bool doMultichannelCheck) override;

protected:
    virtual void processPacket(
        const QHostAddress& discoveryAddr,
        const nx::network::SocketAddress& deviceEndpoint,
        const nx::network::upnp::DeviceInfo& devInfo,
        const QByteArray& xmlDevInfo,
        QnResourceList& result) override;

private:
    static bool vmaxAuthenticate(CLSimpleHTTPClient& client, const QAuthenticator& auth);
    QByteArray readDescriptionPage(CLSimpleHTTPClient& client);
    int getApiPort(const QByteArray& answer) const;

private:
    friend class QnPlVmax480Resource;

    QnMediaServerModule* m_serverModule = nullptr;
    QMap<int, QByteArray> getCamNames(const QByteArray& answer);
};

#endif // #ifdef ENABLE_VMAX480
