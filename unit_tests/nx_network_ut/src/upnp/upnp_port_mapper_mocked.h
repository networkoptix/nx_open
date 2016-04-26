#ifndef UPNP_PORT_MAPPER_MOCKED_H
#define UPNP_PORT_MAPPER_MOCKED_H

#include <thread>

#include <nx/network/upnp/upnp_async_client.h>
#include <nx/network/upnp/upnp_port_mapper.h>
#include <utils/thread/sync_queue.h>

namespace nx_upnp {
namespace test {

//! \class UpnpAsyncClient implementation for tests
class AsyncClientMock
:
    public AsyncClient
{
public:
    explicit AsyncClientMock( const HostAddress& externalIp );
    ~AsyncClientMock() override;

    virtual void externalIp(
        const QUrl& url,
        std::function< void( const HostAddress& ) > callback ) override;

    virtual void addMapping(
        const QUrl& url, const HostAddress& internalIp,
        quint16 internalPort, quint16 externalPort,
        Protocol protocol, const QString& description, quint64 duration,
        std::function< void( bool ) > callback ) override;

    virtual void deleteMapping(
        const QUrl& url, quint16 externalPort, Protocol protocol,
        std::function< void( bool ) > callback ) override;

    virtual void getMapping(
        const QUrl& url, quint32 index,
        std::function< void( MappingInfo ) > callback ) override;

    virtual void getMapping(
        const QUrl& url, quint16 externalPort, Protocol protocol,
        std::function< void( MappingInfo ) > callback ) override;

    typedef std::map<
            std::pair< quint16 /*external*/, Protocol /*protocol*/ >,
            std::pair< SocketAddress /*internal*/, QString /*description*/ >
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
    std::thread m_thread;
    nx::SyncQueue< std::function< void() > > m_tasks;
};

class PortMapperMocked
:
        public PortMapper
{
public:
    PortMapperMocked( const HostAddress& internalIp, const HostAddress& externalIp,
                      quint64 checkMappingsInterval = DEFAULT_CHECK_MAPPINGS_INTERVAL );
    AsyncClientMock& clientMock();
};

} // namespace test
} // namespace nx_upnp

#endif // UPNP_PORT_MAPPER_MOCKED_H
