#ifndef vmax480_resource_searcher_h_1806
#define vmax480_resource_searcher_h_1806

#include "core/resource_managment/resource_searcher.h"
#include "utils/network/nettools.h"

class CLSimpleHTTPClient;

class QnPlVmax480ResourceSearcher : public QnAbstractNetworkResourceSearcher
{
    QnPlVmax480ResourceSearcher();

public:

    ~QnPlVmax480ResourceSearcher();

    static QnPlVmax480ResourceSearcher& instance();

    QnResourceList findResources(void);

    QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters);

    virtual QList<QnResourcePtr> checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck) override;
protected:
    // return the manufacture of the server
    virtual QString manufacture() const;
private:
    QMap<QString, QUdpSocket*> m_socketList;
    QUdpSocket* sockByName(const QnInterfaceAndAddr& iface);
    QMap<int, QByteArray> getCamNames(CLSimpleHTTPClient& client);
    bool vmaxAuthenticate(CLSimpleHTTPClient& client, const QAuthenticator& auth);
};



#endif //vmax480_resource_searcher_h_1806