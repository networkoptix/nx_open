#ifndef upnp_resource_searcher_h_1806
#define upnp_resource_searcher_h_1806

#include <QtCore/QAtomicInt>
#include <QtCore/QElapsedTimer>
#include <QtXml/QXmlDefaultHandler>

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

//!Partial parser for SSDP description xml (UPnP(TM) Device Architecture 1.1, 2.3)
class UpnpDeviceDescriptionSaxHandler : public QXmlDefaultHandler
{
    UpnpDeviceInfo m_deviceInfo;
    QString m_currentElementName;
public:
    virtual bool startDocument()
    {
        return true;
    }

    virtual bool startElement( const QString& /*namespaceURI*/, const QString& /*localName*/, const QString& qName, const QXmlAttributes& /*atts*/ )
    {
        m_currentElementName = qName;
        return true;
    }

    virtual bool characters( const QString& ch )
    {
        if( m_currentElementName == QLatin1String("friendlyName") )
            m_deviceInfo.friendlyName = ch;
        else if( m_currentElementName == QLatin1String("manufacturer") )
            m_deviceInfo.manufacturer = ch;
        else if( m_currentElementName == QLatin1String("modelName") )
            m_deviceInfo.modelName = ch;
        else if( m_currentElementName == QLatin1String("serialNumber") )
            m_deviceInfo.serialNumber = ch;
        else if( m_currentElementName == QLatin1String("presentationURL") )
            m_deviceInfo.presentationUrl = ch;

        return true;
    }

    virtual bool endElement( const QString& /*namespaceURI*/, const QString& /*localName*/, const QString& /*qName*/ )
    {
        m_currentElementName.clear();
        return true;
    }

    virtual bool endDocument()
    {
        return true;
    }

    /*
    QString friendlyName() const { return m_friendlyName; }
    QString manufacturer() const { return m_manufacturer; }
    QString modelName() const { return m_modelName; }
    QString serialNumber() const { return m_serialNumber; }
    QString presentationUrl() const { return m_presentationUrl; }
    */
    UpnpDeviceInfo deviceInfo() const { return m_deviceInfo; }
};

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
        const HostAddress& host,
        const UpnpDeviceInfo& devInfo,
        const QByteArray& xmlDevInfo,
        const QAuthenticator &auth,
        QnResourceList& result) = 0;
private:
    QByteArray getDeviceDescription(const QByteArray& uuidStr, const QUrl& url);
    QHostAddress findBestIface(const HostAddress& host);
    void processSocket(AbstractDatagramSocket* socket, QSet<QByteArray>& processedUuid, QnResourceList& result);
protected:
    void readDeviceXml(
        const QByteArray& uuidStr,
        const QUrl& descritionUrl,
        const HostAddress& sender,
        QnResourceList& result);
    void processDeviceXml(
        const QByteArray& foundDeviceDescription,
        const HostAddress& host,
        const HostAddress& sender,
        QnResourceList& result );
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
        const HostAddress& discoveredDevAddress,
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
        const HostAddress& host,
        const UpnpDeviceInfo& devInfo,
        const QByteArray& xmlDevInfo,
        QnResourceList& result ) = 0;

private:
    QnResourceList m_resList;
};

#endif // upnp_resource_searcher_h_1806
