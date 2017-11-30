#ifndef UPNP_DEVICE_SEARCHER_H
#define UPNP_DEVICE_SEARCHER_H

#include "upnp_device_description.h"

#include <list>
#include <map>
#include <set>

#include <QtCore/QByteArray>
#include <QtCore/QElapsedTimer>
#include <QtCore/QObject>
#include <QtCore/QString>

#include <nx/network/async_stoppable.h>
#include <nx/network/aio/aio_event_handler.h>
#include <nx/network/http/http_types.h>
#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/network/nettools.h>
#include <nx/network/socket.h>
#include <nx/utils/async_operation_guard.h>
#include <nx/utils/atomic_unique_ptr.h>
#include <nx/utils/thread/long_runnable.h>
#include <nx/utils/timer_manager.h>

#include "upnp_search_handler.h"

namespace nx_upnp {

class NX_NETWORK_API AbstractDeviceSearcherSettings
{
public:
    virtual ~AbstractDeviceSearcherSettings() = default;

    virtual int cacheTimeout() const = 0;
    virtual bool isUpnpMulticastEnabled() const = 0;
};

class NX_NETWORK_API DeviceSearcherDefaultSettings:
    public AbstractDeviceSearcherSettings
{
public:
    virtual int cacheTimeout() const override;
    virtual bool isUpnpMulticastEnabled() const override;
};

//!Discovers UPnP devices on network and passes found devices info to registered handlers
/*!
    Searches devices asynchronously
    NOTE: sends discover packets from all local network interfaces
    NOTE: Handlers are iterated in order they were registered
    NOTE: Class methods are thread-safe with the only exception: saveDiscoveredDevicesSnapshot() and processDiscoveredDevices() calls MUST be serialized by calling entity
    NOTE: this class is single-tone
*/
class NX_NETWORK_API DeviceSearcher:
    public QObject,
    public nx::utils::TimerEventHandler,
    public QnStoppable
{
    Q_OBJECT

public:
    static const unsigned int DEFAULT_DISCOVER_TRY_TIMEOUT_MS = 3000;
    static const QString DEFAULT_DEVICE_TYPE;

    /*!
        \param globalSettings Controlls if multicasts should be enabled, always enabled if nullptr.
        \param discoverTryTimeoutMS Timeout between UPnP discover packet dispatch.
    */
    explicit DeviceSearcher(
        const AbstractDeviceSearcherSettings& settings,
        unsigned int discoverTryTimeoutMS = DEFAULT_DISCOVER_TRY_TIMEOUT_MS );
    virtual ~DeviceSearcher();

    //!Implementation of QnStoppable::pleaseStop
    virtual void pleaseStop() override;

    /*!
        If handler is already added for deviceType, nothing is done
        NOTE: Handlers are iterated in order they were registered
        NOTE: if deviceType is empty, all devices will be reported
    */
    void registerHandler( SearchHandler* handler, const QString& deviceType = QString() );

    /*!
     *  If handler is not added for deviceType, nothing is done
     *  NOTE: if deviceType is empty, handler will be unregistred for ALL device types
     *        even if they were registred by separate calls with certain types
     */
    void unregisterHandler( SearchHandler* handler, const QString& deviceType = QString() );

    //!Makes internal copy of discovered but not processed devices. processDiscoveredDevices uses this copy
    void saveDiscoveredDevicesSnapshot();
    //!Passes discovered devices info snapshot to registered handlers
    /*!
        If some handlers processes packet (UPNPSearchHandler::processPacket returns true), then packet is removed and not passed to other handlers
        \param handlerToUse If NULL, all handlers are used, otherwise packets are passed to handlerToUse only
    */
    void processDiscoveredDevices( SearchHandler* handlerToUse = NULL );
    int cacheTimeout() const;

    static DeviceSearcher* instance();

private:
    class DiscoveredDeviceInfo
    {
    public:
        //!Ip address of discovered device (address, UDP datagram came from)
        HostAddress deviceAddress;
        //!Address of local interface, device has been discovered on
        QHostAddress localInterfaceAddress;
        //!Device uuid. Places as a separater member because it becomes known before devInfo
        QByteArray uuid;
        nx::utils::Url descriptionUrl;
        DeviceInfo devInfo;
        QByteArray xmlDevInfo;
    };

    typedef std::map<nx_http::AsyncHttpClientPtr, DiscoveredDeviceInfo> HttpClientsDict;

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

    const AbstractDeviceSearcherSettings& m_settings;
    const unsigned int m_discoverTryTimeoutMS;
    mutable QnMutex m_mutex;
    quint64 m_timerID;
    nx::utils::AsyncOperationGuard m_handlerGuard;
    std::map< QString, std::map< SearchHandler*, uintptr_t > > m_handlers;
    mutable QSet<QnInterfaceAndAddr> m_interfacesCache;
    //map<local interface ip, socket>
    std::map<QString, SocketReadCtx> m_socketList;
    HttpClientsDict m_httpClients;
    //!map<device host address, device info>
    std::map<HostAddress, DiscoveredDeviceInfo> m_discoveredDevices;
    std::map<HostAddress, DiscoveredDeviceInfo> m_discoveredDevicesToProcess;
    std::map<QByteArray, UPNPDescriptionCacheItem> m_upnpDescCache;
    bool m_terminated;
    QElapsedTimer m_cacheTimer;

    nx::utils::AtomicUniquePtr<AbstractDatagramSocket> m_receiveSocket;
    nx::Buffer m_receiveBuffer;
    bool m_needToUpdateReceiveSocket;

    //!Implementation of TimerEventHandler::onTimer
    virtual void onTimer( const quint64& timerID ) override;
    void onSomeBytesRead(
        AbstractCommunicatingSocket* sock,
        SystemError::ErrorCode errorCode,
        nx::Buffer* readBuffer,
        size_t bytesRead );

    void dispatchDiscoverPackets();
    bool needToUpdateReceiveSocket() const;
    nx::utils::AtomicUniquePtr<AbstractDatagramSocket> updateReceiveSocketUnsafe();
    std::shared_ptr<AbstractDatagramSocket> getSockByIntf( const QnInterfaceAndAddr& iface );
    void startFetchDeviceXml(
        const QByteArray& uuidStr,
        const nx::utils::Url& descriptionUrl,
        const HostAddress& sender );
    void processDeviceXml( const DiscoveredDeviceInfo& devInfo, const QByteArray& foundDeviceDescription );
    //QByteArray getDeviceDescription( const QByteArray& uuidStr, const QUrl& url );
    QHostAddress findBestIface( const HostAddress& host );
    /*!
        NOTE: MUST be called with m_mutex locked. Also, returned item MUST be used under same lock
    */
    const UPNPDescriptionCacheItem* findDevDescriptionInCache( const QByteArray& uuid );
    /*!
        NOTE: MUST be called with m_mutex locked
    */
    void updateItemInCache( DiscoveredDeviceInfo devInfo );

    void processPacket(DiscoveredDeviceInfo info);

private slots:
    void onDeviceDescriptionXmlRequestDone( nx_http::AsyncHttpClientPtr httpClient );
};

} // namespace nx_upnp

#endif  //UPNP_DEVICE_SEARCHER_H
