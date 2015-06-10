#ifndef UPNP_PORT_MAPPER_MOCKED_H
#define UPNP_PORT_MAPPER_MOCKED_H

#include <utils/network/upnp/upnp_async_client.h>
#include <utils/network/upnp/upnp_port_mapper.h>

namespace nx_upnp {
namespace test {

//! \class UpnpAsyncClient implementation for tests
class UpnpAsyncClientMock
        : public AsyncClient
{
public:
    UpnpAsyncClientMock( const HostAddress& externalIp );

    virtual
    bool externalIp( const QUrl& url,
                     const std::function< void( const HostAddress& ) >& callback ) override;

    virtual
    bool addMapping( const QUrl& url, const HostAddress& internalIp,
                     quint16 internalPort, quint16 externalPort,
                     Protocol protocol, const QString& description,
                     const std::function< void( bool ) >& callback ) override;

    virtual
    bool deleteMapping( const QUrl& url, quint16 externalPort, Protocol protocol,
                        const std::function< void( bool ) >& callback ) override;

    virtual
    bool getMapping( const QUrl& url, quint32 index,
                     const std::function< void( const MappingInfo& ) >& callback );

    virtual
    bool getMapping( const QUrl& url, quint16 externalPort, Protocol protocol,
                     const std::function< void( const MappingInfo& ) >& callback );

// public for tests:
    const HostAddress m_externalIp;
    const quint16 m_disabledPort;
    std::map< std::pair< quint16 /*port*/, Protocol /*protocol*/ >,
              std::pair< SocketAddress /*external*/, QString /*description*/ > > m_mappings;
};

class UpnpPortMapperMocked
        : public PortMapper
{
public:
    UpnpPortMapperMocked( const HostAddress& internalIp, const HostAddress& externalIp );
    UpnpAsyncClientMock& clientMock();
};

} // namespace test
} // namespace nx_upnp

#endif // UPNP_PORT_MAPPER_MOCKED_H
