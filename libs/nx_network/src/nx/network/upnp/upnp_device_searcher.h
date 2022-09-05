// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

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

namespace nx::network::upnp {

class NX_NETWORK_API AbstractDeviceSearcherSettings
{
public:
    virtual ~AbstractDeviceSearcherSettings() = default;

    virtual int cacheTimeout() const = 0;
};

class NX_NETWORK_API DeviceSearcherDefaultSettings:
    public AbstractDeviceSearcherSettings
{
public:
    virtual int cacheTimeout() const override;
};

/**
 * Discovers UPnP devices on network and passes found devices info to registered handlers.
 * Searches devices asynchronously
 * NOTE: sends discover packets from all local network interfaces
 * NOTE: Handlers are iterated in order they were registered
 * NOTE: this class is single-tone
 */
class NX_NETWORK_API DeviceSearcher:
    public QObject,
    public nx::utils::TimerEventHandler,
    public QnStoppable
{
    Q_OBJECT

public:
    static const unsigned int kDefaultDiscoverTryTimeoutMs = 3000;
    static const QString kDefaultDeviceType;

    /**
     * @param globalSettings Controls if multicasts should be enabled, always enabled if nullptr.
     * @param discoverTryTimeoutMS Timeout between UPnP discover packet dispatch.
     */
    explicit DeviceSearcher(
        nx::utils::TimerManager* timerManager,
        std::unique_ptr<AbstractDeviceSearcherSettings> settings,
        bool ignoreUsb0NetworkInterfaceIfOthersExist = false,
        std::function<bool()> isHttpsForced = nullptr,
        unsigned int discoverTryTimeoutMS = kDefaultDiscoverTryTimeoutMs);

    virtual ~DeviceSearcher();

    virtual void pleaseStop() override;
    void stop();

    /**
     * If handler is already added for deviceType, nothing is done
     * NOTE: Handlers are iterated in order they were registered
     * NOTE: if deviceType is empty, all devices will be reported
     */
    void registerHandler(SearchHandler* handler, const QString& deviceType = QString());

    /**
     *  If handler is not added for deviceType, nothing is done
     *  NOTE: if deviceType is empty, handler will be unregistered for ALL device types
     *  even if they were registered by separate calls with certain types
     */
    void unregisterHandler(SearchHandler* handler, const QString& deviceType = QString());

    int cacheTimeout() const;
    nx::utils::TimerManager* timerManager() const { return m_timerManager; }
private:
    class DiscoveredDeviceInfo
    {
    public:
        /** Ip address of discovered device (address, UDP datagram came from). */
        HostAddress deviceAddress;
        /** Address of local interface, device has been discovered on. */
        QHostAddress localInterfaceAddress;
        /** Device uuid. Places as a separator member because it becomes known before devInfo. */
        QByteArray uuid;
        nx::utils::Url descriptionUrl;
        DeviceInfo devInfo;
        QByteArray xmlDevInfo;
    };

    typedef std::map<nx::network::http::AsyncHttpClientPtr, DiscoveredDeviceInfo> HttpClientsDict;

    class UPNPDescriptionCacheItem
    {
    public:
        DeviceInfo devInfo;
        QByteArray xmlDevInfo;
        qint64 creationTimestamp;

        UPNPDescriptionCacheItem():
            creationTimestamp(0)
        {
        }
    };

    class SocketReadCtx
    {
    public:
        std::shared_ptr<AbstractDatagramSocket> sock;
        nx::Buffer buf;
    };

    std::unique_ptr<AbstractDeviceSearcherSettings> m_settings;
    bool m_ignoreUsb0NetworkInterfaceIfOthersExist;
    std::function<bool()> m_isHttpsForced;
    const unsigned int m_discoverTryTimeoutMS;
    mutable nx::Mutex m_mutex;
    quint64 m_timerID;
    nx::utils::AsyncOperationGuard m_handlerGuard;
    std::map< QString, std::map< SearchHandler*, uintptr_t > > m_handlers;
    mutable QSet<QnInterfaceAndAddr> m_interfacesCache;
    /** map<local interface ip, socket>. */
    std::map<QString, SocketReadCtx> m_socketList;
    HttpClientsDict m_httpClients;
    /** map<device host address, device info>. */
    std::map<HostAddress, DiscoveredDeviceInfo> m_discoveredDevices;
    std::map<HostAddress, DiscoveredDeviceInfo> m_discoveredDevicesToProcess;
    std::map<QByteArray, UPNPDescriptionCacheItem> m_upnpDescCache;
    bool m_terminated;
    QElapsedTimer m_cacheTimer;

    nx::utils::AtomicUniquePtr<AbstractDatagramSocket> m_receiveSocket;
    nx::Buffer m_receiveBuffer;
    bool m_needToUpdateReceiveSocket;
    nx::utils::TimerManager* m_timerManager;
    nx::ReadWriteLock m_stoppingLock;

    virtual void onTimer(const quint64& timerID) override;
    void onSomeBytesRead(
        AbstractCommunicatingSocket* sock,
        SystemError::ErrorCode errorCode,
        nx::Buffer* readBuffer,
        size_t bytesRead);

    void dispatchDiscoverPackets();
    bool needToUpdateReceiveSocket() const;
    nx::utils::AtomicUniquePtr<AbstractDatagramSocket> updateReceiveSocketUnsafe();
    std::shared_ptr<AbstractDatagramSocket> getSockByIntf(const HostAddress& address);
    void startFetchDeviceXml(
        const QByteArray& uuidStr,
        const nx::utils::Url& descriptionUrl,
        const HostAddress& sender);
    void processDeviceXml(const DiscoveredDeviceInfo& devInfo, const nx::Buffer& foundDeviceDescription);
    QHostAddress findBestIface(const HostAddress& host);

    /* NOTE: MUST be called with m_mutex locked. Returned item MUST be used under the same lock. */
    const UPNPDescriptionCacheItem* findDevDescriptionInCache(const QByteArray& uuid);

    /* NOTE: MUST be called with m_mutex locked. */
    void updateItemInCache(DiscoveredDeviceInfo devInfo);

    void processPacket(DiscoveredDeviceInfo info);

private slots:
    void onDeviceDescriptionXmlRequestDone(nx::network::http::AsyncHttpClientPtr httpClient);
};

} // namespace nx::network::upnp

