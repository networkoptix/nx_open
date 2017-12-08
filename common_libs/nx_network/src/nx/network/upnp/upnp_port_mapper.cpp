#include "upnp_port_mapper.h"

#include <random>

#include <QtCore/QDateTime>

#include <nx/utils/log/log.h>

static const size_t BREAK_FAULTS_COUNT = 5; // faults in a row
static const size_t BREAK_TIME_PER_FAULT = 1 * 60; // wait 1 minute per fault in a row
static const size_t BREAK_TIME_MAX = 2 * 60 * 60; // dont wait more than 2 hours

static const quint16 PORT_SAFE_RANGE_BEGIN = 4096; // begining of range to peak rnd port
static const quint16 PORT_SAFE_RANGE_END = 49151;

static const quint16 MAPPING_TIME_RATIO = 10; // 10 times longer then we check

namespace nx_upnp {

PortMapper::PortMapper(
    bool isEnabled,
    quint64 checkMappingsInterval,
    const QString& description,
    const QString& device)
    :
    SearchAutoHandler(device),
    m_isEnabled(isEnabled),
    m_upnpClient(new AsyncClient()),
    m_description(description),
    m_checkMappingsInterval(checkMappingsInterval)
{
    m_timerId = nx::utils::TimerManager::instance()->addTimer(
        this,
        std::chrono::milliseconds(m_checkMappingsInterval));
}

PortMapper::~PortMapper()
{
    quint64 timerId;
    {
        QnMutexLocker lock(&m_mutex);
        timerId = m_timerId;
        m_timerId = 0;
    }

    nx::utils::TimerManager::instance()->joinAndDeleteTimer(timerId);
    m_upnpClient.reset();
}

const quint64 PortMapper::DEFAULT_CHECK_MAPPINGS_INTERVAL = 1 * 60 * 1000; // 10 min

bool PortMapper::enableMapping(
    quint16 port,
    Protocol protocol,
    std::function< void(SocketAddress) > callback)
{
    PortId portId(port, protocol);

    QnMutexLocker lock(&m_mutex);
    if (!m_mapRequests.emplace(std::move(portId), std::move(callback)).second)
        return false; // port already mapped

    if (m_isEnabled)
    {
        // ask to map this port on all known devices
        for (auto& device : m_devices)
            ensureMapping(device.second, port, protocol);
    }

    return true;
}

bool PortMapper::disableMapping(quint16 port, Protocol protocol)
{
    QnMutexLocker lock(&m_mutex);
    const auto it = m_mapRequests.find(PortId(port, protocol));
    if (it == m_mapRequests.end())
        return false;

    removeMapping(it->first);
    m_mapRequests.erase(it);
    return true;
}

void PortMapper::setIsEnabled(bool isEnabled)
{
    QnMutexLocker lock(&m_mutex);
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
    m_lastFail = QDateTime::currentDateTime().toTime_t();
}

bool PortMapper::FailCounter::isOk()
{
    if (m_failsInARow < BREAK_FAULTS_COUNT)
        return true;

    const auto breakTime = m_lastFail + BREAK_TIME_PER_FAULT * m_failsInARow;
    return QDateTime::currentDateTime().toTime_t() > breakTime;
}

PortMapper::PortId::PortId(quint16 port_, Protocol protocol_)
    : port(port_), protocol(protocol_)
{
}

bool PortMapper::PortId::operator < (const PortId& rhs) const
{
    if (port < rhs.port) return true;
    if (port == rhs.port && protocol < rhs.protocol) return true;
    return false;
}

bool PortMapper::processPacket(
    const QHostAddress& localAddress, const SocketAddress& devAddress,
    const DeviceInfo& devInfo, const QByteArray& xmlDevInfo)
{
    static_cast< void >(xmlDevInfo);
    return searchForMappers(HostAddress(localAddress.toString()), devAddress, devInfo);
}

bool PortMapper::searchForMappers(
    const HostAddress& localAddress,
    const SocketAddress& devAddress,
    const DeviceInfo& devInfo)
{
    bool atLeastOneFound = false;
    for (const auto& service: devInfo.serviceList)
    {
        if (service.serviceType != AsyncClient::WAN_IP)
            continue; // uninteresting

        nx::utils::Url url;
        url.setHost(devAddress.address.toString());
        url.setPort(devAddress.port);
        url.setPath(service.controlUrl);

        QnMutexLocker lk(&m_mutex);
        addNewDevice(localAddress, url, devInfo.serialNumber);
    }

    for (const auto& subDev : devInfo.deviceList)
        atLeastOneFound |= searchForMappers(localAddress, devAddress, subDev);

    return atLeastOneFound;
}

void PortMapper::onTimer(const quint64& /*timerID*/)
{
    QnMutexLocker lock(&m_mutex);
    if (m_isEnabled)
    {
        for (auto& device : m_devices)
        {
            updateExternalIp(device.second);
            for (const auto& request : m_mapRequests)
            {
                const auto it = device.second.mapped.find(request.first);
                if (it != device.second.mapped.end())
                {
                    checkMapping(
                        device.second, it->first.port,
                        it->second, it->first.protocol);
                }
                else
                {
                    const auto& portId = request.first;
                    ensureMapping(device.second, portId.port, portId.protocol);
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
        m_timerId = nx::utils::TimerManager::instance()->addTimer(
            this, std::chrono::milliseconds(m_checkMappingsInterval));
    }
}

void PortMapper::addNewDevice(
    const HostAddress& localAddress,
    const nx::utils::Url& url,
    const QString& serial)
{
    const auto itBool = m_devices.emplace(serial, Device());
    if (!itBool.second)
        return; // known device

    Device& newDevice = itBool.first->second;
    newDevice.internalIp = localAddress;
    newDevice.url = url;

    if (m_isEnabled)
    {
        updateExternalIp(newDevice);
        for (const auto map : m_mapRequests)
            ensureMapping(newDevice, map.first.port, map.first.protocol);
    }

    NX_LOGX(lit("New device %1 ( %2 ) has been found on %3")
        .arg(url.toString(QUrl::RemovePassword))
        .arg(serial).arg(localAddress.toString()),
        cl_logDEBUG2);
}

void PortMapper::removeMapping(PortId portId)
{
    for (auto& device: m_devices)
    {
        const auto mapping = device.second.mapped.find(portId);
        if (mapping != device.second.mapped.end())
        {
            m_upnpClient->deleteMapping(
                device.second.url, mapping->second, mapping->first.protocol,
                [this, &device, mapping, portId](bool success)
            {
                if (!success)
                    return;

                QnMutexLocker lk(&m_mutex);
                device.second.mapped.erase(mapping);

                const auto request = m_mapRequests.find(portId);
                if (request == m_mapRequests.end())
                    return;

                SocketAddress address(device.second.externalIp, 0);
                const auto callback = request->second;

                lk.unlock();
                return callback(std::move(address));
            });
        }
    }
}

void PortMapper::updateExternalIp(Device& device)
{
    if (!device.failCounter.isOk())
        return;

    m_upnpClient->externalIp(device.url,
        [this, &device](HostAddress externalIp)
    {
        std::list<Guard> callbackGuards;

        QnMutexLocker lock(&m_mutex);
        NX_LOGX(lit("externalIp='%1' on device %2")
            .arg(externalIp.toString())
            .arg(device.url.toString(QUrl::RemovePassword)), cl_logDEBUG1);

        // All mappings with old IPs are not valid.
        if (device.externalIp != externalIp && device.externalIp != HostAddress())
        {
            for (auto& map: device.mapped)
            {
                const auto it = m_mapRequests.find(map.first);
                if (it != m_mapRequests.end())
                {
                    callbackGuards.push_back(Guard(std::bind(
                        it->second, SocketAddress(device.externalIp, 0))));
                }
            }
        }

        if (externalIp != HostAddress())
            device.failCounter.success();
        else
            device.failCounter.failure();

        device.externalIp.swap(externalIp);
    });
}

void PortMapper::checkMapping(
    Device& device,
    quint16 inPort,
    quint16 exPort,
    Protocol protocol)
{
    if (!device.failCounter.isOk())
        return;

    m_upnpClient->getMapping(
        device.url, exPort, protocol,
        [this, &device, inPort, exPort, protocol](AsyncClient::MappingInfo info)
    {
        QnMutexLocker lk(&m_mutex);
        device.failCounter.success();

        bool stillMapped = (
            info.internalPort == inPort &&
            info.externalPort == exPort &&
            info.protocol == protocol &&
            info.internalIp == device.internalIp);

        PortId portId(inPort, protocol);
        if (!stillMapped)
            device.mapped.erase(device.mapped.find(portId));

        const auto request = m_mapRequests.find(portId);
        if (request == m_mapRequests.end())
        {
            // no need to provide this mapping any longer
            return;
        }

        SocketAddress address(device.externalIp, stillMapped ? exPort : 0);
        const auto callback = request->second;

        if (!stillMapped)
            ensureMapping(device, inPort, protocol);

        lk.unlock();
        if (address.address != HostAddress())
            return callback(std::move(address));
    });
}

// TODO: reduse the size of method
void PortMapper::ensureMapping(Device& device, quint16 inPort, Protocol protocol)
{
    if (!device.failCounter.isOk())
        return;

    m_upnpClient->getAllMappings(device.url,
        [this, &device, inPort, protocol](AsyncClient::MappingList list)
    {
        QnMutexLocker lk(&m_mutex);
        device.failCounter.success();

        // Save existing mappings in case if router can silently remap them
        device.engagedPorts.clear();
        for (const auto mapping : list)
            device.engagedPorts.insert(PortId(mapping.externalPort, mapping.protocol));

        const auto deviceMap = device.mapped.find(PortId(inPort, protocol));
        const auto request = m_mapRequests.find(PortId(inPort, protocol));
        if (request == m_mapRequests.end())
        {
            // no need to provide this mapping any longer
            return;
        }

        const auto callback = request->second;
        for (const auto mapping : list)
            if (mapping.internalIp == device.internalIp &&
                mapping.internalPort == inPort &&
                mapping.protocol == protocol &&
                (mapping.duration == 0 || mapping.duration > m_checkMappingsInterval))
            {
                NX_LOGX(lit("Already mapped %1").arg(mapping.toString()),
                    cl_logDEBUG1);

                if (deviceMap == device.mapped.end() ||
                    deviceMap->second != mapping.externalPort)
                {
                    // the change should be saved and reported
                    device.mapped[PortId(inPort, protocol)] = mapping.externalPort;

                    lk.unlock();
                    if (device.externalIp != HostAddress())
                        callback(SocketAddress(device.externalIp, mapping.externalPort));
                }

                return;
            }

        // to ensure mapping we should create it from scratch
        makeMapping(device, inPort, protocol);

        if (deviceMap != device.mapped.end())
        {
            SocketAddress invalid(device.externalIp, 0);

            // address lose should be saved and reported
            device.mapped.erase(deviceMap);

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
    PORT_SAFE_RANGE_BEGIN, PORT_SAFE_RANGE_END);

// TODO: reduse the size of method
void PortMapper::makeMapping(
    Device& device,
    quint16 inPort,
    Protocol protocol,
    size_t retries)
{
    if (!device.failCounter.isOk())
        return;

    quint16 desiredPort;
    do
        desiredPort = portDistribution(randomEngine);
    while (device.engagedPorts.count(PortId(desiredPort, protocol)));

    m_upnpClient->addMapping(
        device.url, device.internalIp, inPort, desiredPort, protocol, m_description,
        m_checkMappingsInterval * MAPPING_TIME_RATIO,
        [this, &device, inPort, desiredPort, protocol, retries](bool success)
    {
        QnMutexLocker lk(&m_mutex);
        if (!success)
        {
            device.failCounter.failure();

            if (retries > 0)
            {
                makeMapping(device, inPort, protocol, retries - 1);
            }
            else
            {
                NX_LOGX(lit("Cound not forward any port on %1")
                    .arg(device.url.toString(QUrl::RemovePassword)),
                    cl_logERROR);
            }

            return;
        }

        device.failCounter.success();

        // save successful mapping and call callback
        const auto request = m_mapRequests.find(PortId(inPort, protocol));
        if (request == m_mapRequests.end())
        {
            // no need to provide this mapping any longer
            return;
        }

        const auto callback = request->second;
        const auto externalIp = device.externalIp;
        device.mapped[PortId(inPort, protocol)] = desiredPort;
        device.engagedPorts.insert(PortId(desiredPort, protocol));

        lk.unlock();
        if (externalIp != HostAddress())
            callback(SocketAddress(externalIp, desiredPort));
    });
}

} // namespace nx_upnp
