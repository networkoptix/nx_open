#ifndef UPNP_PORT_MAPPER_H
#define UPNP_PORT_MAPPER_H

#include "upnp_device_searcher.h"
#include "upnp_async_client.h"

class UpnpPortMapper
        : UPNPSearchAutoHandler
{
public:
    UpnpPortMapper();

    struct MappingInfo
    {
        quint16     internalPort;
        quint16     externalPort;
        HostAddress externalIp;

        MappingInfo( quint16 inPort, quint16 exPort, const HostAddress& exIp)
            : internalPort( inPort ), externalPort( exPort )
            , externalIp( exIp ) {}
    };
    typedef std::function< void( const MappingInfo& ) > MappingCallback;

    /*! Asks to forward the @param port to some external port
     *  @returns false if @param port has already been mapped */
    bool enableMapping( quint16 port, const MappingCallback& callback );

    /*! Asks to cancel @param port forwarding
     *  @returns false if @param port hasnt been mapped */
    bool disableMapping( quint16 port, bool waitForFinish = true );

private:
    class CallbackControl;
    class MappingDevice;

    void enableMappingOnDevice( MappingDevice& device, quint16 port );

    virtual bool processPacket(
        const QHostAddress& localAddress, const SocketAddress& devAddress,
        const UpnpDeviceInfo& devInfo, const QByteArray& xmlDevInfo ) override;

    bool searchForMappers( const HostAddress& localAddress,
                           const SocketAddress& devAddress,
                           const UpnpDeviceInfo& devInfo );

private:
    QMutex m_mutex;
    UpnpAsyncClient m_upnpClient;

    std::map< quint16, std::shared_ptr< CallbackControl > > m_mappings;
    std::map< QString, std::unique_ptr< MappingDevice > > m_devices;
    // TODO: replace unique_ptr with try_emplace when avaliable
};

#endif // UPNP_PORT_MAPPER_H
