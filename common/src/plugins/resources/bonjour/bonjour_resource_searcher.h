#ifndef bonjour_resource_searcher_h_1806
#define bonjour_resource_searcher_h_1806

#include "core/resource_managment/resource_searcher.h"
#include "utils/network/nettools.h"

class CLSimpleHTTPClient;

class QnBonjourResourceSearcher : public QnAbstractNetworkResourceSearcher
{
public:
    QnBonjourResourceSearcher();
    ~QnBonjourResourceSearcher();
    QnResourceList findResources(void);
protected:
    virtual void processPacket(const QHostAddress& discoveryAddr, const QString& hostAddr, const QString& friendlyName, const QString& manufacturer, const QString& modelName, const QString& serialNumber, QnResourceList& result) = 0;
private:
    QByteArray getDeviceDescription(const QByteArray& uuidStr, const QUrl& url);
private:
    QMap<QString, QUdpSocket*> m_socketList;
    QUdpSocket* sockByName(const QnInterfaceAndAddr& iface);

    QMap<QByteArray, QByteArray> m_deviceXmlCache;
    QTime m_cacheLivetime;
};

#endif //bonjour_resource_searcher_h_1806
