#ifndef vmax480_resource_searcher_h_1806
#define vmax480_resource_searcher_h_1806

#include "plugins/resources/upnp/upnp_resource_searcher.h"

class CLSimpleHTTPClient;

class QnPlVmax480ResourceSearcher : public QnUpnpResourceSearcher
{
    QnPlVmax480ResourceSearcher();

public:

    ~QnPlVmax480ResourceSearcher();

    static QnPlVmax480ResourceSearcher& instance();


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
    virtual QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters) override;
private:
    QMap<int, QByteArray> getCamNames(const QByteArray& answer);
    bool vmaxAuthenticate(CLSimpleHTTPClient& client, const QAuthenticator& auth);
    QByteArray readDescriptionPage(CLSimpleHTTPClient& client);
    int getApiPort(const QByteArray& answer) const;
};


#endif //vmax480_resource_searcher_h_1806
