#ifndef UPNP_PORT_MAPPER_H
#define UPNP_PORT_MAPPER_H

#include "upnp_device_searcher.h"
#include "upnp_async_client.h"

#include "utils/common/app_info.h"

#include <QWaitCondition>

namespace nx_upnp {

class PortMapper
        : SearchAutoHandler
        , TimerEventHandler
{
public:
    PortMapper( quint64 checkMappingsInterval = DEFAULT_CHECK_MAPPINGS_INTERVAL,
                const QString& description = QnAppInfo::organizationName(),
                const QString& device = AsyncClient::INTERNAL_GATEWAY );
    ~PortMapper();

    static const quint64 DEFAULT_CHECK_MAPPINGS_INTERVAL;
    typedef AsyncClient::Protocol Protocol;

    /*!
     * Asks to forward the @param port to some external port
     *
     *  @returns false if @param port has already been mapped
     *  @param callback is called only if smth changes: some port have been mapped,
     *         remapped or mapping is not valid any more)
     */
    bool enableMapping( quint16 port, Protocol protocol,
                        std::function< void( SocketAddress ) > callback );

    /*!
     * Asks to cancel @param port forwarding
     *
     *  @returns false if @param port hasnt been mapped
     */
    bool disableMapping( quint16 port, Protocol protocol );

protected: // for testing only
    class FailCounter
    {
    public:
        FailCounter();
        void success();
        void failure();
        bool isOk();

    private:
        size_t m_failsInARow;
        size_t m_lastFail;
    };

    struct Device
    {
        QUrl url;
        HostAddress internalIp;
        HostAddress externalIp;

        FailCounter failCounter;
        std::map< std::pair< quint16, Protocol >, SocketAddress > mapped;
    };

    virtual bool processPacket(
        const QHostAddress& localAddress, const SocketAddress& devAddress,
        const DeviceInfo& devInfo, const QByteArray& xmlDevInfo ) override;

    bool searchForMappers( const HostAddress& localAddress,
                           const SocketAddress& devAddress,
                           const DeviceInfo& devInfo );

    virtual void onTimer( const quint64& timerID ) override;

private:
    void addNewDevice( const HostAddress& localAddress,
                       const QUrl& url, const QString& serial );

    void updateExternalIp( Device& device );
    void checkMapping( Device& device, quint16 inPort, quint16 exPort, Protocol protocol );
    void ensureMapping( Device& device, quint16 inPort, Protocol protocol );
    void makeMapping( Device& device, quint16 inPort, quint16 desiredPort,
                      Protocol protocol, size_t retries = 5 );

protected: // for testing only
    QMutex m_mutex;
    std::unique_ptr<AsyncClient> m_upnpClient;
    quint64 m_timerId;
    const QString m_description;
    const quint64 m_checkMappingsInterval;

    std::map< std::pair< quint16, Protocol >,
              std::function< void( SocketAddress ) > > m_mapRequests;

    // TODO: replace unique_ptr with try_emplace when avaliable
    std::map< QString, Device > m_devices;
};

} // namespace nx_upnp

#endif // UPNP_PORT_MAPPER_H
