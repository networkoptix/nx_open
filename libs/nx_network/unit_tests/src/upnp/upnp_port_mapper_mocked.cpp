// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "upnp_port_mapper_mocked.h"

namespace nx {
namespace network {
namespace upnp {
namespace test {

AsyncClientMock::AsyncClientMock():
    m_disabledPort(80)
{
    m_thread = nx::utils::thread(
        [this]()
        {
            while (const auto task = m_tasks.pop())
                task();
        });
}

AsyncClientMock::~AsyncClientMock()
{
    m_tasks.push(nullptr);
    m_thread.join();
}

void AsyncClientMock::externalIp(
    const nx::utils::Url& /*url*/,
    std::function< void(const HostAddress&) > callback)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_tasks.push([callback, ip = m_externalIp] { callback(ip); });
}

void AsyncClientMock::addMapping(
    const nx::utils::Url& /*url*/, const HostAddress& internalIp,
    quint16 internalPort, quint16 externalPort,
    Protocol protocol, const QString& description, quint64 /*duration*/,
    std::function< void(bool) > callback)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (externalPort == m_disabledPort ||
        m_mappings.find(std::make_pair(externalPort, protocol))
        != m_mappings.end())
    {
        return m_tasks.push([callback] { callback(false); });
    }

    m_mappings[std::make_pair(externalPort, protocol)]
        = std::make_pair(SocketAddress(internalIp, internalPort), description);

    m_tasks.push([callback] { callback(true); });
}

void AsyncClientMock::deleteMapping(
    const nx::utils::Url& /*url*/, quint16 externalPort, Protocol protocol,
    std::function< void(bool) > callback)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    const bool isErased = m_mappings.erase(std::make_pair(externalPort, protocol));
    m_tasks.push([isErased, callback] { callback(isErased); });
}

void AsyncClientMock::getMapping(
    const nx::utils::Url& /*url*/, quint32 index,
    std::function< void(MappingInfo) > callback)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (m_mappings.size() <= index)
        return m_tasks.push([callback] { callback(MappingInfo()); });

    const auto mapping = *std::next(m_mappings.begin(), index);
    m_tasks.push(
        [callback, mapping]
        {
            callback(MappingInfo(
                mapping.second.first.address,
                mapping.second.first.port,
                mapping.first.first,
                mapping.first.second,
                mapping.second.second));
        });
}

void AsyncClientMock::getMapping(
    const nx::utils::Url& /*url*/, quint16 externalPort, Protocol protocol,
    std::function< void(MappingInfo) > callback)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    const auto it = m_mappings.find(std::make_pair(externalPort, protocol));
    if (it == m_mappings.end())
        return m_tasks.push([callback] { callback(MappingInfo()); });

    const auto mapping = *it;
    m_tasks.push(
        [callback, mapping]
        {
            callback(MappingInfo(
                mapping.second.first.address,
                mapping.second.first.port,
                mapping.first.first,
                mapping.first.second,
                mapping.second.second));
        });
}

void AsyncClientMock::changeExternalIp(const HostAddress ip)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_externalIp = ip;
}

AsyncClientMock::Mappings AsyncClientMock::mappings() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_mappings;
}

size_t AsyncClientMock::mappingsCount() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_mappings.size();
}

bool AsyncClientMock::mkMapping(const Mappings::value_type& value)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_mappings.insert(value).second;
}

bool AsyncClientMock::rmMapping(quint16 port, Protocol protocol)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_mappings.erase(std::make_pair(port, protocol));
}

PortMapperMocked::PortMapperMocked(
    DeviceSearcher* deviceSearcher,
    const HostAddress& internalIp,
    std::chrono::milliseconds checkMappingsInterval)
    :
    PortMapper(deviceSearcher, true, checkMappingsInterval, lit("UpnpPortMapperMocked"), QString())
{
    m_upnpClient.reset(new AsyncClientMock);

    DeviceInfo::Service service;
    service.serviceType = AsyncClient::kWanIp;
    service.controlUrl = lit("/someUrl");

    DeviceInfo devInfo;
    devInfo.serialNumber = lit("XXX");
    devInfo.serviceList.push_back(service);

    searchForMappers(internalIp, SocketAddress(), devInfo);
}

AsyncClientMock& PortMapperMocked::clientMock()
{
    return *static_cast< AsyncClientMock* >(m_upnpClient.get());
}

} // namespace test
} // namespace upnp
} // namespace network
} // namespace nx
