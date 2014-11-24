
#ifndef UPNP_DEVICE_SEARCHER_H
#define UPNP_DEVICE_SEARCHER_H

#include <list>
#include <map>

#include <QtCore/QByteArray>
#include <QtCore/QElapsedTimer>
#include <QtNetwork/QHostAddress>
#include <QtCore/QObject>
#include <QtCore/QMutex>
#include <QtCore/QString>

#include <utils/common/long_runnable.h>
#include <utils/common/stoppable.h>
#include <utils/common/timermanager.h>
#include <utils/network/http/httptypes.h>
#include <utils/network/http/asynchttpclient.h>
#include <utils/network/nettools.h>
#include <utils/network/socket.h>


//Contains some info about discovered UPnP device
struct UpnpDeviceInfo
{
    QString friendlyName;
    QString manufacturer;
    QString modelName;
    QString serialNumber;
    QString presentationUrl;
};

//!Receives discovered devices info
class UPNPSearchHandler
{
public:
    virtual ~UPNPSearchHandler() {}

    /*!
        \param localInterfaceAddress Local interface address, device has been discovered on
        \param discoveredDevAddress Discovered device address
        \param devInfo Parameters, received by parsing \a xmlDevInfo
        \param xmlDevInfo xml data as defined in [UPnP Device Architecture 1.1, section 2.3]
        \return true, if device has been recognized and processed successfully, false otherwise. If true, packet WILL NOT be passed to other processors
    */
    virtual bool processPacket(
        const QHostAddress& localInterfaceAddress,
        const QString& discoveredDevAddress,
        const UpnpDeviceInfo& devInfo,
        const QByteArray& xmlDevInfo ) = 0;
};

//!Discovers UPnP devices on network and passes found devices info to registered handlers
/*!
    Searches devices asynchronously
    \note sends discover packets from all local network interfaces
    \note Handlers are iterated in order they were registered
    \note Class methods are thread-safe with the only exception: \a saveDiscoveredDevicesSnapshot() and \a processDiscoveredDevices() calls MUST be serialized by calling entity
    \note this class is single-tone
*/
class UPNPDeviceSearcher
:
    public QObject,
    public TimerEventHandler,
    public QnStoppable
{
    Q_OBJECT

public:
    static const unsigned int DEFAULT_DISCOVER_TRY_TIMEOUT_MS = 3000;

    /*!
        \param discoverTryTimeoutMS Timeout between UPnP discover packet dispatch
    */
    UPNPDeviceSearcher( unsigned int discoverTryTimeoutMS = DEFAULT_DISCOVER_TRY_TIMEOUT_MS );
    virtual ~UPNPDeviceSearcher();

    //!Implementation of \a QnStoppable::pleaseStop
    virtual void pleaseStop() override;

    /*!
        If \a handler is already added, nothing is done
        \note Handlers are iterated in order they were registered
    */
    void registerHandler( UPNPSearchHandler* handler );
    void cancelHandlerRegistration( UPNPSearchHandler* handler );

    //!Makes internal copy of discovered but not processed devices. \a processDiscoveredDevices uses this copy
    void saveDiscoveredDevicesSnapshot();
    //!Passes discovered devices info snapshot to registered handlers
    /*!
        If some handlers processes packet (UPNPSearchHandler::processPacket returns true), then packet is removed and not passed to other handlers
        \param handlerToUse If NULL, all handlers are used, otherwise packets are passed to \a handlerToUse only
    */
    void processDiscoveredDevices( UPNPSearchHandler* handlerToUse = NULL );

    static UPNPDeviceSearcher* instance();
    static int cacheTimeout();
private:
    class DiscoveredDeviceInfo
    {
    public:
        //!Ip address of discovered device (address, UDP datagram came from)
        QString deviceAddress;
        //!Address of local interface, device has been discovered on
        QHostAddress localInterfaceAddress;
        //!Device uuid. Places as a separater member because it becomes known before \a devInfo
        QByteArray uuid;
        QUrl descriptionUrl;
        UpnpDeviceInfo devInfo;
        QByteArray xmlDevInfo;
    };

    typedef std::map<std::shared_ptr<nx_http::AsyncHttpClient>, DiscoveredDeviceInfo> HttpClientsDict;

    class UPNPDescriptionCacheItem
    {
    public:
        UpnpDeviceInfo devInfo;
        QByteArray xmlDevInfo;
        qint64 creationTimestamp;

        UPNPDescriptionCacheItem()
        :
            creationTimestamp( 0 )
        {
        }
    };

    class SocketReadCtx
    {
    public:
        std::shared_ptr<AbstractDatagramSocket> sock;
        nx::Buffer buf;
    };

    const unsigned int m_discoverTryTimeoutMS;
    mutable QMutex m_mutex;
    quint64 m_timerID;
    std::list<UPNPSearchHandler*> m_handlers;
    //map<local interface ip, socket>
    std::map<QString, SocketReadCtx> m_socketList;
    char* m_readBuf;
    HttpClientsDict m_httpClients;
    std::list<DiscoveredDeviceInfo> m_discoveredDevices;
    std::list<DiscoveredDeviceInfo> m_discoveredDevicesToProcess;
    std::map<QByteArray, UPNPDescriptionCacheItem> m_upnpDescCache;
    bool m_terminated;
    QElapsedTimer m_cacheTimer;

    //!Implementation of \a TimerEventHandler::onTimer
    virtual void onTimer( const quint64& timerID ) override;
    void onSomeBytesRead(
        AbstractCommunicatingSocket* sock,
        SystemError::ErrorCode errorCode,
        nx::Buffer* readBuffer,
        size_t bytesRead ) noexcept;

    void dispatchDiscoverPackets();
    std::shared_ptr<AbstractDatagramSocket> getSockByIntf( const QnInterfaceAndAddr& iface );
    void startFetchDeviceXml( const QByteArray& uuidStr, const QUrl& descriptionUrl, const QString& sender );
    void processDeviceXml( const DiscoveredDeviceInfo& devInfo, const QByteArray& foundDeviceDescription );
    //QByteArray getDeviceDescription( const QByteArray& uuidStr, const QUrl& url );
    QHostAddress findBestIface( const QString& host );
    /*!
        \note MUST be called with \a m_mutex locked. Also, returned item MUST be used under same lock
    */
    const UPNPDescriptionCacheItem* findDevDescriptionInCache( const QByteArray& uuid );
    /*!
        \note MUST be called with \a m_mutex locked
    */
    void updateItemInCache( const DiscoveredDeviceInfo& devInfo );

private slots:
    void onDeviceDescriptionXmlRequestDone( nx_http::AsyncHttpClientPtr httpClient );
};

#endif  //UPNP_DEVICE_SEARCHER_H
