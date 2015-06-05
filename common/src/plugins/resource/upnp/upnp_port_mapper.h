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

    /*! Asks to forward the @param port to some external port */
    bool enableMapping( quint16 port, const MappingCallback& callback );

    /*! Asks to stop @param port forwarding */
    bool disableMapping( quint16 port, bool waitForFinish = true );

private:
    class MappingDevice;
    void enableMappingOnDevice( MappingDevice& device, quint16 port );

    virtual bool processPacket(
        const QHostAddress& localAddress, const SocketAddress& devAddress,
        const UpnpDeviceInfo& devInfo, const QByteArray& xmlDevInfo ) override;

    bool searchForMappers( const HostAddress& localAddress,
                           const SocketAddress& devAddress,
                           const UpnpDeviceInfo& devInfo );

    //! mutex protected callback
    class CallbackControl
    {
    public:
        CallbackControl( const MappingCallback& callback );
        void call( const MappingInfo& info );
        void clear();

    private:
        QMutex m_mutex;
        MappingCallback m_callback;
    };

    //! single mapping device interface
    class MappingDevice
    {
    public:
        MappingDevice( UpnpAsyncClient& upnpClient,
                       const HostAddress& internalIp,
                       const QUrl& url);

        HostAddress externalIp() const;
        void map( quint16 port, const std::function< void( quint16 ) >& callback );
        void unmap( quint16 port );

    private:
        UpnpAsyncClient& m_upnpClient;
        HostAddress m_internalIp;
        QUrl m_url;

        mutable QMutex m_mutex;
        HostAddress m_externalIp;
        // result are waiting here while m_externalIp is not avaliable
        std::map< quint16, std::function< void() > > m_successQueue;
    };

private:
    QMutex m_mutex;
    UpnpAsyncClient m_upnpClient;

    std::map< quint16, std::shared_ptr< CallbackControl > > m_mappings;
    std::map< QString, std::unique_ptr< MappingDevice > > m_devices;
    // TODO: replace unique_ptr with try_emplace when avaliable
};

#endif // UPNP_PORT_MAPPER_H
