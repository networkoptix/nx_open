#pragma once

#include <network/module_information.h>
#include <nx/network/aio/timer.h>
#include <nx/network/system_socket.h>
#include <nx/utils/move_only_func.h>

namespace nx {
namespace vms {
namespace discovery {

/**
 * Multicasts module information and listens for such multicasts to discover VMS modules.
 */
class UdpMulticastFinder:
    public network::aio::BasicPollable
{
public:
    using ModuleHandler = nx::utils::MoveOnlyFunc<void(
        const QnModuleInformationWithAddresses& module, const nx::network::SocketAddress& endpoint)>;

    static const nx::network::SocketAddress kMulticastEndpoint;
    static const std::chrono::milliseconds kUpdateInterfacesInterval;
    static const std::chrono::milliseconds kSendInterval;

    UdpMulticastFinder(network::aio::AbstractAioThread* thread = nullptr);
    void setMulticastEndpoint(nx::network::SocketAddress endpoint);
    void setUpdateInterfacesInterval(std::chrono::milliseconds interval);
    void setSendInterval(std::chrono::milliseconds interval);

    /** Sets moduleInformation to multicast, starts multicast process if not running yet. */
    void multicastInformation(const QnModuleInformationWithAddresses& information);

    /** Listens to module information multicasts, executes handler on each message recieved. */
    void listen(ModuleHandler handler);

    virtual void stopWhileInAioThread() override;
    void updateInterfaces();
    void setIsMulticastEnabledFunction(nx::utils::MoveOnlyFunc<bool()> function);

private:
    typedef std::map<nx::network::HostAddress, std::unique_ptr<network::UDPSocket>> Senders;

    std::unique_ptr<network::UDPSocket> makeSocket(const nx::network::SocketAddress& endpoint);
    void joinMulticastGroup(const nx::network::HostAddress& ip);
    void receiveModuleInformation();
    void sendModuleInformation(Senders::iterator senderIterator);

private:
    nx::network::SocketAddress m_multicastEndpoint;
    std::chrono::milliseconds m_updateInterfacesInterval;
    std::chrono::milliseconds m_sendInterval;
    nx::utils::MoveOnlyFunc<bool()> m_isMulticastEnabledFunction;

    network::aio::Timer m_updateTimer;
    Buffer m_ownModuleInformation;
    Senders m_senders;

    Buffer m_inData;
    std::unique_ptr<network::UDPSocket> m_receiver;
    ModuleHandler m_moduleHandler;
};

} // namespace discovery
} // namespace vms
} // namespace nx
