#ifndef vmax480_resource_searcher_h_1806
#define vmax480_resource_searcher_h_1806

#include "plugins/resources/bonjour/bonjour_resource_searcher.h"

class CLSimpleHTTPClient;

class QnPlVmax480ResourceSearcher : public QnBonjourResourceSearcher
{
    QnPlVmax480ResourceSearcher();

public:

    ~QnPlVmax480ResourceSearcher();

    static QnPlVmax480ResourceSearcher& instance();


    virtual void processPacket(const QHostAddress& discoveryAddr, const QString& host, const BonjurDeviceInfo& devInfo, QnResourceList& result) override;

    virtual QList<QnResourcePtr> checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck) override;
protected:
    // return the manufacture of the server
    virtual QString manufacture() const;
    virtual QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters) override;
private:
    QMap<int, QByteArray> getCamNames(CLSimpleHTTPClient& client);
    bool vmaxAuthenticate(CLSimpleHTTPClient& client, const QAuthenticator& auth);
};


#endif //vmax480_resource_searcher_h_1806
