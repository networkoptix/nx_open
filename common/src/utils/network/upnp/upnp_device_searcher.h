#ifndef UPNP_DEVICE_SEARCHER_H
#define UPNP_DEVICE_SEARCHER_H

#include "upnp_device_description.h"

#include <list>
#include <map>
#include <set>

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

namespace nx_upnp {

//!Receives discovered devices info
class SearchHandler
{
public:
    virtual ~SearchHandler() {}

    /*!
        \param localInterfaceAddress Local interface address, device has been discovered on
        \param discoveredDevAddress Discovered device address
        \param devInfo Parameters, received by parsing \a xmlDevInfo
        \param xmlDevInfo xml data as defined in [UPnP Device Architecture 1.1, section 2.3]
        \return true, if device has been recognized and processed successfully, false otherwise. If true, packet WILL NOT be passed to other processors
    */
    virtual bool processPacket(
        const QHostAddress& localInterfaceAddress,
        const SocketAddress& discoveredDevAddress,
        const DeviceInfo& devInfo,
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
class DeviceSearcher
:
    public QObject,
    public TimerEventHandler,
    public QnStoppable
{
    Q_OBJECT

public:
    static const unsigned int DEFAULT_DISCOVER_TRY_TIMEOUT_MS = 3000;
    static const QString DEFAULT_DEVICE_TYPE;

    /*!
        \param discoverDeviceTypes Devies to discover, mediaservers by default
        \param discoverTryTimeoutMS Timeout between UPnP discover packet dispatch
    */
    DeviceSearcher( const QString& discoverDeviceType = DEFAULT_DEVICE_TYPE,
                        unsigned int discoverTryTimeoutMS = DEFAULT_DISCOVER_TRY_TIMEOUT_MS );
    virtual ~DeviceSearcher();

    //!Implementation of \a QnStoppable::pleaseStop
    virtual void pleaseStop() override;

    /*!
        If \a handler is already added, nothing is done
        \note Handlers are iterated in order they were registered
    */
    void registerHandler( SearchHandler* handler );
    void cancelHandlerRegistration( SearchHandler* handler );

    // TODO: merge into registerHandler
    void registerDeviceType( const QString& devType );
    void cancelDeviceTypeRegistration( const QString& devType );

    //!Makes internal copy of discovered but not processed devices. \a processDiscoveredDevices uses this copy
    void saveDiscoveredDevicesSnapshot();
    //!Passes discovered devices info snapshot to registered handlers
    /*!
        If some handlers processes packet (UPNPSearchHandler::processPacket returns true), then packet is removed and not passed to other handlers
        \param handlerToUse If NULL, all handlers are used, otherwise packets are passed to \a handlerToUse only
    */
    void processDiscoveredDevices( SearchHandler* handlerToUse = NULL );

    static DeviceSearcher* instance();
    static int cacheTimeout();
private:
    class DiscoveredDeviceInfo
    {
    public:
        //!Ip address of discovered device (address, UDP datagram came from)
        HostAddress deviceAddress;
        //!Address of local interface, device has been discovered on
        QHostAddress localInterfaceAddress;
        //!Device uuid. Places as a separater member because it becomes known before \a devInfo
        QByteArray uuid;
        QUrl descriptionUrl;
        DeviceInfo devInfo;
        QByteArray xmlDevInfo;
    };

    typedef std::map<std::shared_ptr<nx_http::AsyncHttpClient>, DiscoveredDeviceInfo> HttpClientsDict;

    class UPNPDescriptionCacheItem
    {
    public:
        DeviceInfo devInfo;
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

    std::set<QString> m_deviceTypes;
    const unsigned int m_discoverTryTimeoutMS;
    mutable QMutex m_mutex;
    quint64 m_timerID;
    std::set<SearchHandler*> m_handlers;
    //map<local interface ip, socket>
    std::map<QString, SocketReadCtx> m_socketList;
    char* m_readBuf;
    HttpClientsDict m_httpClients;
    //!map<device host address, device info>
    std::map<HostAddress, DiscoveredDeviceInfo> m_discoveredDevices;
    std::map<HostAddress, DiscoveredDeviceInfo> m_discoveredDevicesToProcess;
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
    void startFetchDeviceXml(
        const QByteArray& uuidStr,
        const QUrl& descriptionUrl,
        const HostAddress& sender );
    void processDeviceXml( const DiscoveredDeviceInfo& devInfo, const QByteArray& foundDeviceDescription );
    //QByteArray getDeviceDescription( const QByteArray& uuidStr, const QUrl& url );
    QHostAddress findBestIface( const HostAddress& host );
    /*!
        \note MUST be called with \a m_mutex locked. Also, returned item MUST be used under same lock
    */
    const UPNPDescriptionCacheItem* findDevDescriptionInCache( const QByteArray& uuid );
    /*!
        \note MUST be called with \a m_mutex locked
    */
    void updateItemInCache( const DiscoveredDeviceInfo& devInfo );
    bool processPacket( const QHostAddress& localInterfaceAddress,
                        const SocketAddress& discoveredDevAddress,
                        const DeviceInfo& devInfo,
                        const QByteArray& xmlDevInfo );

private slots:
    void onDeviceDescriptionXmlRequestDone( nx_http::AsyncHttpClientPtr httpClient );
};

class SearchAutoHandler
        : public SearchHandler
{
public:
    SearchAutoHandler(const QString& devType = QString());
    virtual ~SearchAutoHandler();

private:
    QString m_devType;
};

} // namespace nx_upnp

#endif  //UPNP_DEVICE_SEARCHER_H
