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

    struct MappingInfo
    {
        quint16     internalPort;
        quint16     externalPort;
        HostAddress externalIp;
        Protocol    protocol;

        MappingInfo( quint16 inPort = 0, quint16 exPort = 0,
                     const HostAddress& exIp = HostAddress(),
                     Protocol prot = Protocol::TCP );
    };

    /*!
     * Asks to forward the @param port to some external port
     *
     *  @returns false if @param port has already been mapped
     *  @param callback is called only if smth changes: some port have been mapped,
     *         remapped or mapping is not valid any more)
     */
    bool enableMapping( quint16 port, Protocol protocol,
                        std::function< void( MappingInfo ) > callback );

    /*!
     * Asks to cancel @param port forwarding
     *
     *  @returns false if @param port hasnt been mapped
     *  @param waitForFinish enforces function to block until callback is finished
     */
    bool disableMapping( quint16 port, Protocol protocol, bool waitForFinish = true );

protected: // for testing only
    class Callback;
    class Device;

    void enableMappingOnDevice( Device& device, bool isCheck,
                                std::pair< quint16, Protocol > request );

    virtual bool processPacket(
        const QHostAddress& localAddress, const SocketAddress& devAddress,
        const DeviceInfo& devInfo, const QByteArray& xmlDevInfo ) override;

    bool searchForMappers( const HostAddress& localAddress,
                           const SocketAddress& devAddress,
                           const DeviceInfo& devInfo );

    virtual void onTimer( const quint64& timerID ) override;

protected: // for testing only
    QMutex m_mutex;
    std::unique_ptr<AsyncClient> m_upnpClient;
    size_t m_asyncInProgress;
    QWaitCondition m_asyncCondition;
    quint64 m_timerId;
    const QString m_description;
    const quint64 m_checkMappingsInterval;

    std::map< std::pair< quint16, Protocol >, std::shared_ptr< Callback > > m_mappings;
    std::map< QString, std::unique_ptr< Device > > m_devices;
    // TODO: replace unique_ptr with try_emplace when avaliable
};

} // namespace nx_upnp

#endif // UPNP_PORT_MAPPER_H
