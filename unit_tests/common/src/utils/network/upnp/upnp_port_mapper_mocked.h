#ifndef UPNP_PORT_MAPPER_MOCKED_H
#define UPNP_PORT_MAPPER_MOCKED_H

#include <utils/network/upnp/upnp_async_client.h>
#include <utils/network/upnp/upnp_port_mapper.h>

namespace nx_upnp {
namespace test {

//! \class UpnpAsyncClient implementation for tests
class AsyncClientMock
        : public AsyncClient
{
public:
    AsyncClientMock( const HostAddress& externalIp );

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

    typedef std::map<
            std::pair< quint16 /*port*/, Protocol /*protocol*/ >,
            std::pair< SocketAddress /*external*/, QString /*description*/ >
        > Mappings;

    Mappings mappings() const;
    size_t mappingsCount() const;
    bool mkMapping( const Mappings::value_type& value );
    bool rmMapping( quint16 port, Protocol protocol );

private:
    const HostAddress m_externalIp;
    const quint16 m_disabledPort;

    mutable QMutex m_mutex;
    Mappings m_mappings;
};

class PortMapperMocked
        : public PortMapper
{
public:
    PortMapperMocked( const HostAddress& internalIp, const HostAddress& externalIp,
                      quint64 checkMappingsInterval = DEFAULT_CHECK_MAPPINGS_INTERVAL );
    AsyncClientMock& clientMock();
};

} // namespace test
} // namespace nx_upnp

#endif // UPNP_PORT_MAPPER_MOCKED_H
