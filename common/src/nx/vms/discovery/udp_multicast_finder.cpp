#include "udp_multicast_finder.h"

#include <nx/fusion/serialization/json.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace vms {
namespace discovery {

static const int kMaxDatagramSize(1500); // ~MTU
static const SocketAddress kMulticastEndpoint("239.255.11.11:5008");
static const std::chrono::milliseconds kUpdateInterval = std::chrono::minutes(1);
static const std::chrono::milliseconds kSendInterval = std::chrono::seconds(10);

UdpMulticastFinder::UdpMulticastFinder(network::aio::AbstractAioThread* thread)
    : network::aio::BasicPollable(thread)
{
    m_updateTimer.bindToAioThread(getAioThread());
}

/** Sets moduleInformation to multicast, starts multicast process if not running yet. */
void UdpMulticastFinder::multicastInformation(
    const QnModuleInformationWithAddresses& information)
{
    m_updateTimer.post(
        [this, information = QJson::serialized(information)]() mutable
        {
            m_ownModuleInformation = std::move(information);
            for (const auto& i: m_interfaces)
                i.second->updateData(&m_ownModuleInformation);
        });
}

/** Listens to module information multicasts, executes handler on each message recieved. */
void UdpMulticastFinder::listen(
    nx::utils::MoveOnlyFunc<void(const QnModuleInformationWithAddresses& module)> handler)
{
    m_updateTimer.post(
        [this, handler = std::move(handler)]() mutable
        {
            m_foundModuleHandler = std::move(handler);
        });
}

void UdpMulticastFinder::stopWhileInAioThread()
{
    m_updateTimer.pleaseStopSync();
    m_interfaces.clear();
}

void UdpMulticastFinder::updateInterfaces()
{
    std::set<HostAddress> localIpList;
    for (const auto& ip: getLocalIpV4AddressList())
        localIpList.insert(ip);

    for (auto it = m_interfaces.begin(); it != m_interfaces.end(); )
    {
        if (localIpList.find(it->first) == localIpList.end())
            it = m_interfaces.erase(it);
        else
            ++it;
    }

    for (const auto address: localIpList)
    {
        if (m_interfaces.find(address) == m_interfaces.end())
            makeInterface(address);
    }
}

void UdpMulticastFinder::makeInterface(const HostAddress& ip)
{
    const auto iterator = m_interfaces.emplace(ip, std::unique_ptr<Interface>()).first;
    auto& interface = iterator->second;

    interface = std::make_unique<Interface>(
        getAioThread(), ip, [this, iterator] { m_interfaces.erase(iterator); });

    interface->updateData(&m_ownModuleInformation);
    interface->readForever(
        [this](const Buffer& buffer)
        {
            QnModuleInformationWithAddresses moduleInformation;
            if (!QJson::deserialize(buffer, &moduleInformation))
                NX_LOGX("Unable to deserialize", cl_logWARNING);
            else
                m_foundModuleHandler(moduleInformation);
        });
}

UdpMulticastFinder::Interface::Interface(
    network::aio::AbstractAioThread* thread,
    const HostAddress& localIp,
    nx::utils::MoveOnlyFunc<void()> errorHandler)
:
    m_errorHandler(std::move(errorHandler))
{
    m_socket.bindToAioThread(thread);
    if (!m_socket.setNonBlockingMode(true) || !m_socket.setReuseAddrFlag(true)
        || !m_socket.bind(SocketAddress(localIp, kMulticastEndpoint.port)))
    {
        m_socket.post(
            [this, code = SystemError::getLastOSErrorCode()] { handleError("init", code); });
    }

    NX_LOGX(lm("New socket %1").str(m_socket.getLocalAddress()), cl_logINFO);
}

void UdpMulticastFinder::Interface::updateData(const Buffer* data)
{
    m_outData = data;
    sendData();
}

void UdpMulticastFinder::Interface::readForever(
    nx::utils::MoveOnlyFunc<void(const Buffer&)> handler)
{
    m_newDataHandler = std::move(handler);
    readData();
}

void UdpMulticastFinder::Interface::sendData()
{
    m_socket.cancelIOSync(network::aio::etTimedOut);
    if (m_outData->isEmpty())
    {
        NX_LOGX("No data to send, waiting...", cl_logDEBUG1);
        m_socket.registerTimer(kSendInterval, [this]{ sendData(); });
        return;
    }

    m_socket.sendToAsync(*m_outData, kMulticastEndpoint,
        [this](SystemError::ErrorCode code, SocketAddress, size_t)
        {
            if (code != SystemError::noError)
                return handleError("send", code);

            NX_LOGX("Successful sent", cl_logDEBUG1);
            m_socket.registerTimer(kSendInterval, [this]{ sendData(); });
        });
}

void UdpMulticastFinder::Interface::readData()
{
    m_inData.reserve(kMaxDatagramSize); // ~ MTU
    m_inData.resize(0);
    m_socket.recvFromAsync(&m_inData,
        [this](SystemError::ErrorCode code, SocketAddress endpoint, size_t size)
        {
            if (code != SystemError::noError)
                return handleError("recv", code);

            NX_LOGX(lm("From %1 got: %2").strs(endpoint, m_inData), cl_logDEBUG2);
            m_newDataHandler(m_inData);
            readData();
        });
}

void UdpMulticastFinder::Interface::handleError(
    const char* stage, SystemError::ErrorCode code)
{
    NX_LOGX(lm("Error on %1: %2").strs(stage, SystemError::toString(code)), cl_logWARNING);
    moveAndCall(m_errorHandler);
}

} // namespace discovery
} // namespace vms
} // namespace nx
