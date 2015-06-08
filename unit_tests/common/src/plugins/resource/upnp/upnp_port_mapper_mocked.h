#ifndef UPNP_PORT_MAPPER_MOCKED_H
#define UPNP_PORT_MAPPER_MOCKED_H

#include <plugins/resource/upnp/upnp_async_client.h>
#include <plugins/resource/upnp/upnp_port_mapper.h>

//! \class UpnpAsyncClient implementation for tests
class UpnpAsyncClientMock
        : public UpnpAsyncClient
{
public:
    UpnpAsyncClientMock( const HostAddress& externalIp );

    virtual
    bool externalIp( const QUrl& url,
                     const std::function< void( const HostAddress& ) >& callback ) override;

    virtual
    bool addMapping( const QUrl& url, const HostAddress& internalIp,
                     quint16 internalPort, quint16 externalPort, const QString& protocol,
                     const std::function< void( bool ) >& callback ) override;

    virtual
    bool deleteMapping( const QUrl& url, quint16 externalPort, const QString& protocol,
                        const std::function< void( bool ) >& callback ) override;

// public for tests:
    const HostAddress m_externalIp;
    const quint16 m_disabledPort;
    std::map<quint16, SocketAddress> m_mappings;
};

class UpnpPortMapperMocked
        : public UpnpPortMapper
{
public:
    UpnpPortMapperMocked( const HostAddress& internalIp, const HostAddress& externalIp );
    UpnpAsyncClientMock& clientMock();
};

#endif // UPNP_PORT_MAPPER_MOCKED_H
