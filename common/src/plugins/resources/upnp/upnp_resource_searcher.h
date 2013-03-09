#ifndef upnp_resource_searcher_h_1806
#define upnp_resource_searcher_h_1806

#include "core/resource_managment/resource_searcher.h"
#include "utils/network/nettools.h"
#include "utils/network/socket.h"
#include <QAtomicInt>

class CLSimpleHTTPClient;

struct UpnpDeviceInfo
{
    QString friendlyName;
    QString manufacturer;
    QString modelName;
    QString serialNumber;
    QString presentationUrl;
};

class QnUpnpResourceSearcher : public QnAbstractNetworkResourceSearcher
{
public:
    QnUpnpResourceSearcher();
    ~QnUpnpResourceSearcher();
    QnResourceList findResources(void);

    void setSendRequests(bool value);
protected:
    virtual void processPacket(const QHostAddress& discoveryAddr, const QString& host, const UpnpDeviceInfo& devInfo, QnResourceList& result) = 0;
private:
    QByteArray getDeviceDescription(const QByteArray& uuidStr, const QUrl& url);
    QHostAddress findBestIface(const QString& host);
private:
    QMap<QString, UDPSocket*> m_socketList;
    UDPSocket* sockByName(const QnInterfaceAndAddr& iface);

    QMap<QByteArray, QByteArray> m_deviceXmlCache;
    QTime m_cacheLivetime;
    bool m_sendRequests;
    UDPSocket* m_receiveSocket;

    static QAtomicInt m_isntanceCnt;
};

#endif // upnp_resource_searcher_h_1806
