#ifndef UPNP_PORT_MAPPER_H
#define UPNP_PORT_MAPPER_H

#include "upnp_device_searcher.h"
#include "upnp_async_client.h"

#include "utils/common/app_info.h"

#include <QWaitCondition>

class UpnpPortMapper
        : UPNPSearchAutoHandler
        , TimerEventHandler
{
public:
    UpnpPortMapper( const QString& description = QnAppInfo::organizationName(),
                    const QString& device = UpnpAsyncClient::INTERNAL_GATEWAY );
    ~UpnpPortMapper();

    typedef UpnpAsyncClient::Protocol Protocol;

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
                        const std::function< void( const MappingInfo& ) >& callback );

    /*!
     * Asks to cancel @param port forwarding
     *
     *  @returns false if @param port hasnt been mapped
     *  @param waitForFinish enforces function to block until callback is finished
     */
    bool disableMapping( quint16 port, Protocol protocol, bool waitForFinish = true );

protected: // for testing only
    class CallbackControl;
    class MappingDevice;

    void enableMappingOnDevice( MappingDevice& device,
                                std::pair< quint16, Protocol > request );

    virtual bool processPacket(
        const QHostAddress& localAddress, const SocketAddress& devAddress,
        const UpnpDeviceInfo& devInfo, const QByteArray& xmlDevInfo ) override;

    bool searchForMappers( const HostAddress& localAddress,
                           const SocketAddress& devAddress,
                           const UpnpDeviceInfo& devInfo );

    virtual void onTimer( const quint64& timerID ) override;

protected: // for testing only
    QMutex m_mutex;
    std::unique_ptr<UpnpAsyncClient> m_upnpClient;
    size_t m_asyncInProgress;
    QWaitCondition m_asyncCondition;
    quint64 m_timerId;
    const QString m_description;

    std::map< std::pair< quint16, Protocol >, std::shared_ptr< CallbackControl > > m_mappings;
    std::map< QString, std::unique_ptr< MappingDevice > > m_devices;
    // TODO: replace unique_ptr with try_emplace when avaliable
};

#endif // UPNP_PORT_MAPPER_H
