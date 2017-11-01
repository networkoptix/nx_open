#ifndef UPNP_PORT_MAPPER_H
#define UPNP_PORT_MAPPER_H

#include <QWaitCondition>

#include <nx/utils/scope_guard.h>
#include <nx/utils/timer_manager.h>

#include "upnp_async_client.h"
#include "upnp_search_handler.h"

namespace nx_upnp {

class NX_NETWORK_API PortMapper
        : SearchAutoHandler
        , nx::utils::TimerEventHandler
{
public:
    PortMapper( bool isEnabled = true,
                quint64 checkMappingsInterval = DEFAULT_CHECK_MAPPINGS_INTERVAL,
                const QString& description = QString(),
                const QString& device = AsyncClient::INTERNAL_GATEWAY );
    ~PortMapper();

    PortMapper( const PortMapper& ) = delete;
    PortMapper( PortMapper&& ) = delete;
    PortMapper& operator = ( const PortMapper& ) = delete;
    PortMapper& operator = ( PortMapper&& ) = delete;

    static const quint64 DEFAULT_CHECK_MAPPINGS_INTERVAL;
    typedef AsyncClient::Protocol Protocol;

    /**
     * Asks to forward the @param port to some external port
     *
     *  @returns false if @param port has already been mapped
     *  @param callback is called only if smth changes: some port have been mapped,
     *         remapped or mapping is not valid any more)
     */
    bool enableMapping( quint16 port, Protocol protocol,
                        std::function< void( SocketAddress ) > callback );

    /**
     * Asks to cancel @param port forwarding
     *
     *  @returns false if @param port hasnt been mapped
     */
    bool disableMapping( quint16 port, Protocol protocol );

    /**
     * Enables/disables actual mapping on devices.
     * When mapper goes disabled no new mappings will be made as well as no old
     *  one will be sustained.
     * When mapper goes enabled all mappings will be resored by timer (including
     *  added during disabled period).
     */
    void setIsEnabled( bool isEnabled );

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

    struct PortId
    {
        quint16 port;
        Protocol protocol;

        PortId( quint16 port_, Protocol protocol_ );
        bool operator < ( const PortId& rhs ) const;
    };

    struct Device
    {
        nx::utils::Url url;
        HostAddress internalIp;
        HostAddress externalIp;

        FailCounter failCounter;
        std::map< PortId, quint16 > mapped; //!< internal port -> external port
        std::set< PortId > engagedPorts;    //!< external ports
    };

    virtual bool processPacket(
        const QHostAddress& localAddress, const SocketAddress& devAddress,
        const DeviceInfo& devInfo, const QByteArray& xmlDevInfo ) override;

    bool searchForMappers( const HostAddress& localAddress,
                           const SocketAddress& devAddress,
                           const DeviceInfo& devInfo );

    virtual void onTimer( const quint64& timerID ) override;

private:
    void addNewDevice(
        const HostAddress& localAddress,
        const nx::utils::Url &url, const QString& serial );

    void removeMapping( PortId portId );
    void updateExternalIp( Device& device );
    void checkMapping( Device& device, quint16 inPort, quint16 exPort, Protocol protocol );
    void ensureMapping( Device& device, quint16 inPort, Protocol protocol );
    void makeMapping( Device& device, quint16 inPort, Protocol protocol, size_t retries = 5 );

protected: // for testing only
    QnMutex m_mutex;
    bool m_isEnabled;
    std::unique_ptr<AsyncClient> m_upnpClient;
    quint64 m_timerId;
    const QString m_description;
    const quint64 m_checkMappingsInterval;

    std::map< PortId, std::function< void( SocketAddress ) > > m_mapRequests;
    std::map< QString, Device > m_devices;
};

} // namespace nx_upnp

#endif // UPNP_PORT_MAPPER_H
