#ifndef vmax480_resource_searcher_h_1806
#define vmax480_resource_searcher_h_1806

#ifdef ENABLE_VMAX

#include "plugins/resource/upnp/upnp_resource_searcher.h"

class CLSimpleHTTPClient;

class QnPlVmax480ResourceSearcher : public QnUpnpResourceSearcherAsync
{
public:

    QnPlVmax480ResourceSearcher();
    ~QnPlVmax480ResourceSearcher();

    static void initStaticInstance( QnPlVmax480ResourceSearcher* _instance );
    static QnPlVmax480ResourceSearcher* instance();


    virtual void processPacket(
        const QHostAddress& discoveryAddr,
        const QString& host,
        const UpnpDeviceInfo& devInfo,
        const QByteArray& xmlDevInfo,
        QnResourceList& result ) override;

    virtual QList<QnResourcePtr> checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck) override;
protected:
    // return the manufacture of the server
    virtual QString manufacture() const;
    virtual QnResourcePtr createResource(const QnUuid &resourceTypeId, const QnResourceParams& params) override;
private:
    QMap<int, QByteArray> getCamNames(const QByteArray& answer);
    bool vmaxAuthenticate(CLSimpleHTTPClient& client, const QAuthenticator& auth);
    QByteArray readDescriptionPage(CLSimpleHTTPClient& client);
    int getApiPort(const QByteArray& answer) const;
};

#endif // #ifdef ENABLE_VMAX480
#endif //vmax480_resource_searcher_h_1806
