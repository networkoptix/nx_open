#ifndef upnp_resource_searcher_h_1806
#define upnp_resource_searcher_h_1806

#include <QtCore/QAtomicInt>
#include <QtCore/QElapsedTimer>
#include <QtXml/QXmlDefaultHandler>

#include "core/resource_management/resource_searcher.h"
#include <nx/network/nettools.h>
#include <nx/network/socket.h>

#include <nx/network/upnp/upnp_device_searcher.h>


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
    QnUpnpResourceSearcher(QnCommonModule* commonModule);
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
        const nx::network::HostAddress& host,
        const nx::network::upnp::DeviceInfo& devInfo,
        const QByteArray& xmlDevInfo,
        const QAuthenticator &auth,
        QnResourceList& result) = 0;

private:
    QByteArray getDeviceDescription(const QByteArray& uuidStr, const QUrl& url);
    QHostAddress findBestIface(const nx::network::HostAddress& host);
    void processSocket(nx::network::AbstractDatagramSocket* socket, QSet<QByteArray>& processedUuid, QnResourceList& result);

protected:
    void readDeviceXml(
        const QByteArray& uuidStr,
        const QUrl& descritionUrl,
        const nx::network::HostAddress& sender,
        QnResourceList& result);
    void processDeviceXml(
        const QByteArray& foundDeviceDescription,
        const nx::network::HostAddress& host,
        const nx::network::HostAddress& sender,
        QnResourceList& result );

private:
    QMap<QString, nx::network::AbstractDatagramSocket*> m_socketList;
    nx::network::AbstractDatagramSocket* sockByName(const nx::network::QnInterfaceAndAddr& iface);

    QMap<QByteArray, QByteArray> m_deviceXmlCache;
    QElapsedTimer m_cacheLivetime;
    nx::network::AbstractDatagramSocket* m_receiveSocket;
};


//!\a QnUpnpResourceSearcher-compatible wrapper for \a UPNPSearchHandler
class QnUpnpResourceSearcherAsync
:
    virtual public QnAbstractNetworkResourceSearcher,
    public nx::network::upnp::SearchHandler
{
public:
    QnUpnpResourceSearcherAsync(QnCommonModule* commonModule);

    //!Implementation of QnAbstractNetworkResourceSearcher::findResources
    virtual QnResourceList findResources() override;
    //!Implementation of UPNPSearchHandler::processPacket
    virtual bool processPacket(
        const QHostAddress& localInterfaceAddress,
        const nx::network::SocketAddress& discoveredDevAddress,
        const nx::network::upnp::DeviceInfo& devInfo,
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
        const nx::network::SocketAddress& deviceEndpoint,
        const nx::network::upnp::DeviceInfo& devInfo,
        const QByteArray& xmlDevInfo,
        QnResourceList& result ) = 0;

private:
    QnResourceList m_resList;
};

#endif // upnp_resource_searcher_h_1806
