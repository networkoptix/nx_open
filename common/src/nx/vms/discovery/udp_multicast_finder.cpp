#include "udp_multicast_finder.h"

#include <nx/fusion/serialization/json.h>
#include <nx/utils/log/log.h>
#include <nx/vms/api/data/module_information.h>

namespace nx {
namespace vms {
namespace discovery {

static const int kMaxDatagramSize(1500); //< Expected MTU size with a little reserve.

const nx::network::SocketAddress UdpMulticastFinder::kMulticastEndpoint("239.255.11.11:5008");
const std::chrono::milliseconds UdpMulticastFinder::kUpdateInterfacesInterval = std::chrono::minutes(1);
const std::chrono::milliseconds UdpMulticastFinder::kSendInterval = std::chrono::seconds(10);

UdpMulticastFinder::UdpMulticastFinder(nx::network::aio::AbstractAioThread* thread):
    network::aio::BasicPollable(thread),
    m_multicastEndpoint(kMulticastEndpoint),
    m_updateInterfacesInterval(kUpdateInterfacesInterval),
    m_sendInterval(kSendInterval)
{
    m_updateTimer.bindToAioThread(getAioThread());
    m_inData.reserve(kMaxDatagramSize);
}

void UdpMulticastFinder::setMulticastEndpoint(nx::network::SocketAddress endpoint)
{
    m_multicastEndpoint = std::move(endpoint);
}

void UdpMulticastFinder::setUpdateInterfacesInterval(std::chrono::milliseconds interval)
{
    m_updateInterfacesInterval = interval;
}

void UdpMulticastFinder::setSendInterval(std::chrono::milliseconds interval)
{
    m_sendInterval = interval;
}

void UdpMulticastFinder::multicastInformation(
    const nx::vms::api::ModuleInformationWithAddresses& information)
{
    m_updateTimer.post(
        [this, information = QJson::serialized(information)]() mutable
        {
            NX_DEBUG(this, lm("Set module information: %1").arg(information));
            m_ownModuleInformation = std::move(information);
            for (auto it = m_senders.begin(); it != m_senders.end(); ++it)
                sendModuleInformation(it);
        });
}

void UdpMulticastFinder::listen(ModuleHandler handler)
{
    m_updateTimer.post(
        [this, handler = std::move(handler)]() mutable
        {
            NX_DEBUG(this, "Listening handler");
            m_moduleHandler = std::move(handler); //< Module handler will be called during recv.
        });
}

void UdpMulticastFinder::stopWhileInAioThread()
{
    m_updateTimer.pleaseStopSync();
    m_senders.clear();
    m_receiver.reset();
}

void UdpMulticastFinder::updateInterfaces()
{
    std::set<nx::network::HostAddress> localIpList;
    for (const auto& ip: nx::network::getLocalIpV4AddressList())
        localIpList.insert(ip);

    NX_DEBUG(this, lm("Update interfaces to %1").container(localIpList));
    for (auto it = m_senders.begin(); it != m_senders.end(); )
    {
        if (localIpList.find(it->first) == localIpList.end())
        {
            it->second->cancelIOSync(network::aio::etNone);
            it = m_senders.erase(it);
        }
        else
        {
            ++it;
        }
    }

    for (const auto& ip: localIpList)
    {
        const auto insert = m_senders.emplace(ip, std::unique_ptr<network::UDPSocket>());
        if (insert.second)
        {
            insert.first->second = makeSocket({ip});
            if (insert.first->second)
            {
                joinMulticastGroup(ip);
                if (!m_ownModuleInformation.isEmpty())
                    sendModuleInformation(insert.first);
            }
            else
            {
                //< Will be fixed in next updateInterfaces().
                m_senders.erase(insert.first);
            }
        }
    }

    if (!m_receiver)
    {
        m_receiver = makeSocket({nx::network::HostAddress::anyHost, m_multicastEndpoint.port});
        for (const auto& ip: localIpList)
            joinMulticastGroup(ip);

        receiveModuleInformation();
    }

    NX_VERBOSE(this, lm("Schedule update interfaces in %1").arg(m_updateInterfacesInterval));
    m_updateTimer.start(m_updateInterfacesInterval, [this](){ updateInterfaces(); });
}

void UdpMulticastFinder::setIsMulticastEnabledFunction(utils::MoveOnlyFunc<bool()> function)
{
    m_updateTimer.dispatch(
        [this, function = std::move(function)]() mutable
        {
            m_isMulticastEnabledFunction = std::move(function);
        });
}

std::unique_ptr<network::UDPSocket> UdpMulticastFinder::makeSocket(const nx::network::SocketAddress& endpoint)
{
    auto socket = std::make_unique<network::UDPSocket>();
    socket->bindToAioThread(getAioThread());

    if (socket->setNonBlockingMode(true)
        && socket->setReuseAddrFlag(true) && socket->bind(endpoint))
    {
        NX_DEBUG(this, lm("New socket %1").arg(socket->getLocalAddress()));
        return socket;
    }

    NX_DEBUG(this, lm("Failed to create socket %1: %2").args(
        endpoint, SystemError::getLastOSErrorText()));

    return nullptr;
}

void UdpMulticastFinder::joinMulticastGroup(const nx::network::HostAddress& ip)
{
    if (!m_receiver)
        return; //< Will be fixed in updateInterfaces().

    if (m_receiver->joinGroup(m_multicastEndpoint.address.toString(), ip.toString()))
    {
        NX_DEBUG(this, lm("Joined group %1 on %2").args(m_multicastEndpoint.address, ip));
        return; //< Ok.
    }

    m_receiver.reset();
    NX_DEBUG(this, lm("Could not join socket %1 to multicast group: %2").args(
        ip, SystemError::getLastOSErrorText()));
}

void UdpMulticastFinder::receiveModuleInformation()
{
    if (!m_receiver)
        return; //< Will be fixed in updateInterfaces().

    m_inData.resize(0);
    m_receiver->recvFromAsync(&m_inData,
        [this](SystemError::ErrorCode code, nx::network::SocketAddress endpoint, size_t /*size*/)
        {
            if (code != SystemError::noError)
            {
                NX_WARNING(this, lm("Failed to read reciever: %2").args(
                    SystemError::toString(code)));

                m_receiver.reset();
                return;
            }

            NX_VERBOSE(this, lm("From %1 got: %2").args(endpoint, m_inData));
            nx::vms::api::ModuleInformationWithAddresses moduleInformation;
            if (!QJson::deserialize(m_inData, &moduleInformation))
            {
                NX_WARNING(this, lm("From %1 unable to deserialize: %2").args(endpoint, m_inData));
            }
            else
            {
                if (moduleInformation.remoteAddresses.size() && m_moduleHandler)
                    m_moduleHandler(moduleInformation, endpoint);
            }

            receiveModuleInformation();
        });
}

void UdpMulticastFinder::sendModuleInformation(Senders::iterator senderIterator)
{
    const auto socket = senderIterator->second.get();
    socket->cancelIOSync(network::aio::etNone);
    if (m_isMulticastEnabledFunction && !m_isMulticastEnabledFunction())
    {
        NX_DEBUG(this, lm("Multicasts are disabled by function"));
        return socket->registerTimer(m_sendInterval,
            [this, senderIterator](){ sendModuleInformation(senderIterator); });
    }

    socket->sendToAsync(m_ownModuleInformation, m_multicastEndpoint,
        [this, senderIterator, socket](SystemError::ErrorCode code, nx::network::SocketAddress, size_t)
        {
            if (code == SystemError::noError)
            {
                NX_VERBOSE(this, lm("Successfully sent from %1 to %2").args(
                    socket->getLocalAddress(), m_multicastEndpoint));

                socket->registerTimer(m_sendInterval,
                    [this, senderIterator](){ sendModuleInformation(senderIterator); });
            }
            else
            {
                NX_WARNING(this, lm("Failed to send from %1 to %2: %3").args(
                    socket->getLocalAddress(), m_multicastEndpoint, SystemError::toString(code)));

                m_senders.erase(senderIterator);
            }
        });
}

} // namespace discovery
} // namespace vms
} // namespace nx
