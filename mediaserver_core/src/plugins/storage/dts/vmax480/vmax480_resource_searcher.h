#ifndef vmax480_resource_searcher_h_1806
#define vmax480_resource_searcher_h_1806

#ifdef ENABLE_VMAX

#include "plugins/resource/upnp/upnp_resource_searcher.h"

class CLSimpleHTTPClient;

class QnPlVmax480ResourceSearcher : public QnUpnpResourceSearcherAsync
{
public:

    QnPlVmax480ResourceSearcher(QnCommonModule* commonModule);
    ~QnPlVmax480ResourceSearcher();

    virtual void processPacket(
        const QHostAddress& discoveryAddr,
        const nx::network::SocketAddress& deviceEndpoint,
        const nx::network::upnp::DeviceInfo& devInfo,
        const QByteArray& xmlDevInfo,
        QnResourceList& result ) override;

    virtual QList<QnResourcePtr> checkHostAddr(const nx::utils::Url& url, const QAuthenticator& auth, bool doMultichannelCheck) override;
protected:
    // return the manufacture of the server
    virtual QString manufacture() const;
    virtual QnResourcePtr createResource(const QnUuid &resourceTypeId, const QnResourceParams& params) override;
private:
    friend class QnPlVmax480Resource;

    QMap<int, QByteArray> getCamNames(const QByteArray& answer);
    static bool vmaxAuthenticate(CLSimpleHTTPClient& client, const QAuthenticator& auth);
    QByteArray readDescriptionPage(CLSimpleHTTPClient& client);
    int getApiPort(const QByteArray& answer) const;
};

#endif // #ifdef ENABLE_VMAX480
#endif //vmax480_resource_searcher_h_1806
