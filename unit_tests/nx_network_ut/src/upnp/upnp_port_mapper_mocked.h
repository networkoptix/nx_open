#pragma once

#include <nx/network/upnp/upnp_async_client.h>
#include <nx/network/upnp/upnp_port_mapper.h>
#include <nx/utils/std/thread.h>
#include <nx/utils/thread/sync_queue.h>
#include <nx/utils/thread/mutex.h>

namespace nx {
namespace network {
namespace upnp {
namespace test {

/**
 * UpnpAsyncClient implementation for tests.
 */
class AsyncClientMock:
    public AsyncClient
{
public:
    AsyncClientMock();
    ~AsyncClientMock() override;

    virtual void externalIp(
        const nx::utils::Url& url,
        std::function< void(const HostAddress&) > callback) override;

    virtual void addMapping(
        const nx::utils::Url& url, const HostAddress& internalIp,
        quint16 internalPort, quint16 externalPort,
        Protocol protocol, const QString& description, quint64 duration,
        std::function< void(bool) > callback) override;

    virtual void deleteMapping(
        const nx::utils::Url& url, quint16 externalPort, Protocol protocol,
        std::function< void(bool) > callback) override;

    virtual void getMapping(
        const nx::utils::Url& url, quint32 index,
        std::function< void(MappingInfo) > callback) override;

    virtual void getMapping(
        const nx::utils::Url& url, quint16 externalPort, Protocol protocol,
        std::function< void(MappingInfo) > callback) override;

    typedef std::map<
        std::pair< quint16 /*external*/, Protocol /*protocol*/ >,
        std::pair< SocketAddress /*internal*/, QString /*description*/ >
    > Mappings;

    void changeExternalIp(const HostAddress ip);
    Mappings mappings() const;
    size_t mappingsCount() const;
    bool mkMapping(const Mappings::value_type& value);
    bool rmMapping(quint16 port, Protocol protocol);

private:
    const quint16 m_disabledPort;

    mutable QnMutex m_mutex;
    HostAddress m_externalIp;
    Mappings m_mappings;
    nx::utils::thread m_thread;
    nx::utils::SyncQueue< std::function< void() > > m_tasks;
};

class PortMapperMocked:
    public PortMapper
{
public:
    PortMapperMocked(
        const HostAddress& internalIp,
        quint64 checkMappingsInterval = DEFAULT_CHECK_MAPPINGS_INTERVAL);
    AsyncClientMock& clientMock();
};

} // namespace test
} // namespace upnp
} // namespace network
} // namespace nx
