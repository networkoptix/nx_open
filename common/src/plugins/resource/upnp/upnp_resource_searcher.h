#ifndef upnp_resource_searcher_h_1806
#define upnp_resource_searcher_h_1806

#include <QtCore/QAtomicInt>
#include <QtCore/QElapsedTimer>

#include "core/resource_management/resource_searcher.h"
#include "utils/network/nettools.h"
#include "utils/network/socket.h"

#include "upnp_device_searcher.h"


//struct UpnpDeviceInfo
//{
//    QString friendlyName;
//    QString manufacturer;
//    QString modelName;
//    QString serialNumber;
//    QString presentationUrl;
//};

class QnUpnpResourceSearcher : virtual public QnAbstractNetworkResourceSearcher
{
public:
    QnUpnpResourceSearcher();
    ~QnUpnpResourceSearcher();

    void setSendRequests(bool value);

protected:
    virtual QnResourceList findResources() override;
    /*!
        \param discoveryAddr Local interface address, device has been discovered on
        \param host Discovered device address
        \param devInfo Parameters, received by parsing \a xmlDevInfo
        \param xmlDevInfo xml data as defined in [UPnP Device Architecture 1.1, section 2.3]
        \param auth Authentication data
    */
    virtual void processPacket(
        const QHostAddress& discoveryAddr,
        const QString& host,
        const UpnpDeviceInfo& devInfo,
        const QByteArray& xmlDevInfo,
        const QAuthenticator &auth,
        QnResourceList& result) = 0;
private:
    QByteArray getDeviceDescription(const QByteArray& uuidStr, const QUrl& url);
    QHostAddress findBestIface(const QString& host);
    void processSocket(AbstractDatagramSocket* socket, QSet<QByteArray>& processedUuid, QnResourceList& result);
protected:
    void readDeviceXml(const QByteArray& uuidStr, const QUrl& descritionUrl, const QString& sender, QnResourceList& result);
    void processDeviceXml(const QByteArray& foundDeviceDescription, const QString& host, const QString& sender, QnResourceList& result);
private:
    QMap<QString, AbstractDatagramSocket*> m_socketList;
    AbstractDatagramSocket* sockByName(const QnInterfaceAndAddr& iface);

    QMap<QByteArray, QByteArray> m_deviceXmlCache;
    QElapsedTimer m_cacheLivetime;
    AbstractDatagramSocket* m_receiveSocket;
};


//!\a QnUpnpResourceSearcher-compatible wrapper for \a UPNPSearchHandler
class QnUpnpResourceSearcherAsync
:
    virtual public QnAbstractNetworkResourceSearcher,
    public UPNPSearchHandler
{
public:
    QnUpnpResourceSearcherAsync();
    virtual ~QnUpnpResourceSearcherAsync();

    //!Implementation of QnAbstractNetworkResourceSearcher::findResources
    virtual QnResourceList findResources() override;
    //!Implementation of UPNPSearchHandler::processPacket
    virtual bool processPacket(
        const QHostAddress& localInterfaceAddress,
        const QString& discoveredDevAddress,
        const UpnpDeviceInfo& devInfo,
        const QByteArray& xmlDevInfo ) override;

protected:
    /*!
        \param discoveryAddr Local interface address, device has been discovered on
        \param host Discovered device address
        \param devInfo Parameters, received by parsing \a xmlDevInfo
        \param xmlDevInfo xml data as defined in [UPnP Device Architecture 1.1, section 2.3]
    */
    virtual void processPacket(
        const QHostAddress& discoveryAddr,
        const QString& host,
        const UpnpDeviceInfo& devInfo,
        const QByteArray& xmlDevInfo,
        QnResourceList& result ) = 0;

private:
    QnResourceList m_resList;
};

#endif // upnp_resource_searcher_h_1806
