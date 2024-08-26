// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "upnp_port_mapper.h"

#include <random>

#include <QtCore/QDateTime>

#include <nx/utils/log/log.h>
#include "upnp_device_searcher.h"

namespace nx::network::upnp {

namespace {

static constexpr size_t kBreakFaultsCount = 5; //< Faults in a row.
static constexpr size_t kBreakTimePerFaultSec = 1 * 60; //< Wait 1 minute per fault in a row.

// The range to peak random port.
static constexpr quint16 kPortSafeRangeBegin = 4096;
static constexpr quint16 kPortSafeRangeEnd = 49151; //< The end is included.

static constexpr int kMappingTimeRatio = 10; //< 10 times longer then we check.

} // namespace

PortMapper::PortMapper(
    nx::network::upnp::DeviceSearcher* deviceSearcher,
    bool isEnabled,
    std::chrono::milliseconds checkMappingsInterval,
    const QString& description,
    const QString& device)
    :
    SearchAutoHandler(deviceSearcher, device),
    m_isEnabled(isEnabled),
    m_upnpClient(new AsyncClient()),
    m_description(description),
    m_checkMappingsInterval(checkMappingsInterval)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    // NOTE: onTimer can be invoked before returned value is saved to m_timerId.
    m_timerId = deviceSearcher->timerManager()->addTimer(
        this, std::chrono::milliseconds(m_checkMappingsInterval));
}

PortMapper::~PortMapper()
{
    quint64 timerId = 0;
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        std::swap(m_timerId, timerId);
    }

    deviceSearcher()->timerManager()->joinAndDeleteTimer(timerId);
    m_upnpClient.reset();
}

bool PortMapper::enableMapping(
    quint16 port,
    Protocol protocol,
    std::function< void(SocketAddress) > callback)
{
    PortId portId(port, protocol);

    NX_MUTEX_LOCKER lock(&m_mutex);
    if (!m_mapRequests.emplace(std::move(portId), std::move(callback)).second)
        return false; //< port already mapped

    if (m_isEnabled)
    {
        // Ask to map this port on all known devices.
        for (auto& device: m_devices)
            ensureMapping(device.second.get(), port, protocol);
    }

    return true;
}

bool PortMapper::disableMapping(quint16 port, Protocol protocol)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    const auto it = m_mapRequests.find(PortId(port, protocol));
    if (it == m_mapRequests.end())
        return false;

    removeMapping(it->first);
    m_mapRequests.erase(it);
    return true;
}

bool PortMapper::isEnabled() const
{
    return m_isEnabled;
}

void PortMapper::setIsEnabled(bool isEnabled)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_isEnabled = isEnabled;
}

PortMapper::FailCounter::FailCounter():
    m_failsInARow(0),
    m_lastFail(0)
{
}

void PortMapper::FailCounter::success()
{
    m_failsInARow = 0;
}

void PortMapper::FailCounter::failure()
{
    ++m_failsInARow;
    m_lastFail = QDateTime::currentDateTime().toSecsSinceEpoch();
}

bool PortMapper::FailCounter::isOk()
{
    if (m_failsInARow < kBreakFaultsCount)
        return true;

    const size_t breakTimeSec = m_lastFail + kBreakTimePerFaultSec * m_failsInARow;
    return QDateTime::currentDateTime().toSecsSinceEpoch() > (qint64) breakTimeSec;
}

PortMapper::PortId::PortId(quint16 port_, Protocol protocol_)
    : port(port_), protocol(protocol_)
{
}

bool PortMapper::PortId::operator< (const PortId& rhs) const
{
    if (port < rhs.port) return true;
    if (port == rhs.port && protocol < rhs.protocol) return true;
    return false;
}

bool PortMapper::processPacket(
    const QHostAddress& localAddress, const SocketAddress& devAddress,
    const DeviceInfo& devInfo, const QByteArray& /*xmlDevInfo*/)
{
    return searchForMappers(
        HostAddress(localAddress.toString().toStdString()),
        devAddress,
        devInfo);
}

bool PortMapper::searchForMappers(
    const HostAddress& localAddress,
    const SocketAddress& devAddress,
    const DeviceInfo& devInfo)
{
    bool atLeastOneFound = false;
    for (const auto& service: devInfo.serviceList)
    {
        if (service.serviceType != AsyncClient::kWanIp)
            continue; // uninteresting

        nx::utils::Url url;
        url.setHost(devAddress.address.toString());
        url.setPort(devAddress.port);
        url.setPath(service.controlUrl);

        NX_MUTEX_LOCKER lk(&m_mutex);
        addNewDevice(localAddress, url, devInfo.serialNumber);
    }

    for (const auto& subDev : devInfo.deviceList)
        atLeastOneFound |= searchForMappers(localAddress, devAddress, subDev);

    return atLeastOneFound;
}

void PortMapper::onTimer(const quint64& /*timerId*/)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (m_isEnabled)
    {
        for (auto& device: m_devices)
        {
            updateExternalIp(device.second.get());
            for (const auto& request : m_mapRequests)
            {
                const auto it = device.second->mapped.find(request.first);
                if (it != device.second->mapped.end())
                {
                    checkMapping(
                        device.second.get(), it->first.port,
                        it->second, it->first.protocol);
                }
                else
                {
                    const auto& portId = request.first;
                    ensureMapping(device.second.get(), portId.port, portId.protocol);
                }
            }
        }
    }
    else
    {
        for (const auto& request : m_mapRequests)
            removeMapping(request.first);
    }

    if (m_timerId)
    {
        m_timerId = deviceSearcher()->timerManager()->addTimer(
            this, std::chrono::milliseconds(m_checkMappingsInterval));
    }
}

void PortMapper::addNewDevice(
    const HostAddress& localAddress,
    const nx::utils::Url& url,
    const QString& serial)
{
    const auto itBool = m_devices.emplace(serial, std::make_unique<Device>());
    if (!itBool.second)
        return; // known device

    Device* newDevice = itBool.first->second.get();
    newDevice->internalIp = localAddress;
    newDevice->url = url;

    if (m_isEnabled)
    {
        updateExternalIp(newDevice);
        for (const auto& map : m_mapRequests)
            ensureMapping(newDevice, map.first.port, map.first.protocol);
    }

    NX_VERBOSE(this, nx::format("New device %1 (%2) has been found on %3").args(
        url.toString(QUrl::RemovePassword),
        serial,
        localAddress));
}

void PortMapper::removeMapping(PortId portId)
{
    for (auto& device: m_devices)
    {
        const auto mapping = device.second->mapped.find(portId);
        if (mapping != device.second->mapped.end())
        {
            m_upnpClient->deleteMapping(
                device.second->url, mapping->second, mapping->first.protocol,
                [this, device = device.second.get(), mapping, portId](bool success)
                {
                    if (!success)
                        return;

                    NX_MUTEX_LOCKER lk(&m_mutex);
                    device->mapped.erase(mapping);

                    const auto request = m_mapRequests.find(portId);
                    if (request == m_mapRequests.end())
                        return;

                    SocketAddress address(device->externalIp, 0);
                    const auto callback = request->second;

                    lk.unlock();
                    return callback(std::move(address));
                });
            }
    }
}

void PortMapper::updateExternalIp(Device* device)
{
    if (!device->failCounter.isOk())
        return;

    m_upnpClient->externalIp(device->url,
        [this, device](HostAddress externalIp)
        {
            std::list<nx::utils::Guard> callbackGuards;

            NX_MUTEX_LOCKER lock(&m_mutex);
            NX_DEBUG(this, nx::format("externalIp='%1' on device %2").args(
                externalIp, device->url.toString(QUrl::RemovePassword)));

            // All mappings with old IPs are not valid.
            if (device->externalIp != externalIp && device->externalIp != HostAddress())
            {
                for (auto& map: device->mapped)
                {
                    const auto it = m_mapRequests.find(map.first);
                    if (it != m_mapRequests.end())
                    {
                        callbackGuards.push_back(nx::utils::Guard(std::bind(
                            it->second, SocketAddress(device->externalIp, 0))));
                    }
                }
            }

            if (externalIp != HostAddress())
                device->failCounter.success();
            else
                device->failCounter.failure();

            device->externalIp.swap(externalIp);
        });
}

void PortMapper::checkMapping(
    Device* device,
    quint16 inPort,
    quint16 exPort,
    Protocol protocol)
{
    if (!device->failCounter.isOk())
        return;

    m_upnpClient->getMapping(
        device->url, exPort, protocol,
        [this, device, inPort, exPort, protocol](AsyncClient::MappingInfo info)
        {
            NX_MUTEX_LOCKER lk(&m_mutex);
            device->failCounter.success();

            bool stillMapped = (
                info.internalPort == inPort &&
                info.externalPort == exPort &&
                info.protocol == protocol &&
                info.internalIp == device->internalIp);

            PortId portId(inPort, protocol);
            if (!stillMapped)
                device->mapped.erase(device->mapped.find(portId));

            const auto request = m_mapRequests.find(portId);
            if (request == m_mapRequests.end())
            {
                // no need to provide this mapping any longer
                return;
            }

            const auto externalIp = device->externalIp;
            const auto callback = request->second;

            if (!stillMapped)
                ensureMapping(device, inPort, protocol);

            lk.unlock();
            if (externalIp != HostAddress())
                return callback(SocketAddress(externalIp, stillMapped ? exPort : 0));
        });
}

// TODO: reduce the size of method
void PortMapper::ensureMapping(Device* device, quint16 inPort, Protocol protocol)
{
    if (!device->failCounter.isOk())
        return;

    m_upnpClient->getAllMappings(device->url,
        [this, device, inPort, protocol](AsyncClient::MappingList list)
        {
            NX_MUTEX_LOCKER lk(&m_mutex);
            device->failCounter.success();

            // Save existing mappings in case if router can silently remap them
            device->engagedPorts.clear();
            for (const auto& mapping : list)
                device->engagedPorts.insert(PortId(mapping.externalPort, mapping.protocol));

            const auto deviceMap = device->mapped.find(PortId(inPort, protocol));
            const auto request = m_mapRequests.find(PortId(inPort, protocol));
            if (request == m_mapRequests.end())
            {
                // no need to provide this mapping any longer
                return;
            }

            const auto callback = request->second;
            for (const auto& mapping : list)
                if (mapping.internalIp == device->internalIp && mapping.internalPort == inPort
                    && mapping.protocol == protocol
                    && (mapping.duration == std::chrono::milliseconds::zero()
                    || mapping.duration > m_checkMappingsInterval))
                {
                    NX_DEBUG(this, nx::format("Already mapped %1").arg(mapping.toString()));

                    if (deviceMap == device->mapped.end() ||
                        deviceMap->second != mapping.externalPort)
                    {
                        // the change should be saved and reported
                        device->mapped[PortId(inPort, protocol)] = mapping.externalPort;

                        lk.unlock();
                        if (device->externalIp != HostAddress())
                            callback(SocketAddress(device->externalIp, mapping.externalPort));
                    }

                    return;
                }

            // to ensure mapping we should create it from scratch
            makeMapping(device, inPort, protocol);

            if (deviceMap != device->mapped.end())
            {
                const auto invalid = device->externalIp == HostAddress()
                    ? SocketAddress()
                    : SocketAddress(device->externalIp, 0);

                // address lose should be saved and reported
                device->mapped.erase(deviceMap);

                lk.unlock();
                if (invalid.address != HostAddress())
                    callback(invalid);
            }
        });
}

// TODO: move into function below when supported
static std::default_random_engine randomEngine(
    std::chrono::system_clock::now().time_since_epoch().count());
static std::uniform_int_distribution<quint16> portDistribution(
    kPortSafeRangeBegin, kPortSafeRangeEnd);

// TODO: reduce the size of method
void PortMapper::makeMapping(
    Device* device,
    quint16 inPort,
    Protocol protocol,
    size_t retries)
{
    if (!device->failCounter.isOk())
        return;

    quint16 desiredPort;
    do
        desiredPort = portDistribution(randomEngine);
    while (device->engagedPorts.count(PortId(desiredPort, protocol)));

    m_upnpClient->addMapping(
        device->url, device->internalIp, inPort, desiredPort, protocol, m_description,
        std::chrono::milliseconds(m_checkMappingsInterval).count() * kMappingTimeRatio,
        [this, device, inPort, desiredPort, protocol, retries](bool success)
        {
            NX_MUTEX_LOCKER lk(&m_mutex);
            if (!success)
            {
                device->failCounter.failure();

                if (retries > 0)
                {
                    makeMapping(device, inPort, protocol, retries - 1);
                }
                else
                {
                    NX_ERROR(this, nx::format("Could not forward any port on %1")
                        .arg(device->url.toString(QUrl::RemovePassword)));
                }

                return;
            }

            device->failCounter.success();

            // save successful mapping and call callback
            const auto request = m_mapRequests.find(PortId(inPort, protocol));
            if (request == m_mapRequests.end())
            {
                // no need to provide this mapping any longer
                return;
            }

            const auto callback = request->second;
            const auto externalIp = device->externalIp;
            device->mapped[PortId(inPort, protocol)] = desiredPort;
            device->engagedPorts.insert(PortId(desiredPort, protocol));

            lk.unlock();
            if (externalIp != HostAddress())
                callback(SocketAddress(externalIp, desiredPort));
        });
}

} // namespace nx::network::upnp
