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

void UdpMulticastFinder::createReceiver()
{
    NX_DEBUG(this, "Creating receiver on %1:%2",
        nx::network::HostAddress::anyHost, m_multicastEndpoint.port);
    m_receiver = makeSocket({nx::network::HostAddress::anyHost, m_multicastEndpoint.port});
    for (const auto& [ip, sock]: m_senders)
        joinMulticastGroup(ip);

    receiveModuleInformation();
}

void UdpMulticastFinder::removeObsoleteSenders()
{
    std::set<nx::network::HostAddress> localIpList = nx::network::getLocalIpV4HostAddressList();

    for (auto it = m_senders.begin(); it != m_senders.end(); )
    {
        if (localIpList.find(it->first) == localIpList.end())
        {
            it->second->cancelIOSync(network::aio::etNone);
            NX_DEBUG(this, "Deleted obsolete sender: %1", it->first);
            it = m_senders.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void UdpMulticastFinder::addNewSenders()
{
    for (const auto& ip: nx::network::getLocalIpV4HostAddressList())
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
}

void UdpMulticastFinder::updateInterfacesInAioThread()
{
    std::set<nx::network::HostAddress> localIpList = nx::network::getLocalIpV4HostAddressList();
    if (!m_receiver)
        createReceiver();

    NX_DEBUG(this, lm("Update interfaces to %1").container(localIpList));
    removeObsoleteSenders();
    addNewSenders();

    NX_VERBOSE(this, lm("Schedule update interfaces in %1").arg(m_updateInterfacesInterval));
    m_updateTimer.start(m_updateInterfacesInterval, [this](){ updateInterfacesInAioThread(); });
}

void UdpMulticastFinder::updateInterfaces()
{
    // Make sure everything is called inside bound thread.
    m_updateTimer.dispatch([this]() { updateInterfacesInAioThread(); });
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
    NX_DEBUG(this, "Could not join socket %1 to multicast group: %2",
        ip, SystemError::getLastOSErrorText());
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

    // Work-around to support dynamic port allocation for testing purposes.
    auto multicastEndpoint = m_multicastEndpoint;
    if (multicastEndpoint.port == 0) {
        NX_ASSERT(m_receiver);
        multicastEndpoint.port = m_receiver->getLocalAddress().port;
    }

    socket->sendToAsync(m_ownModuleInformation, multicastEndpoint,
        [this, senderIterator, socket](
            SystemError::ErrorCode code, nx::network::SocketAddress destination, size_t)
        {
            if (code == SystemError::noError)
            {
                NX_VERBOSE(this, lm("Successfully sent from %1 to %2").args(
                    socket->getLocalAddress(), destination));

                socket->registerTimer(m_sendInterval,
                    [this, senderIterator](){ sendModuleInformation(senderIterator); });
            }
            else
            {
                NX_WARNING(this, lm("Failed to send from %1 to %2: %3").args(
                    socket->getLocalAddress(), destination, SystemError::toString(code)));

                m_senders.erase(senderIterator);
            }
    });
}

} // namespace discovery
} // namespace vms
} // namespace nx
