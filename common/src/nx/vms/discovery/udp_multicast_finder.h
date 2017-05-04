#pragma once

#include <network/module_information.h>
#include <nx/network/aio/timer.h>
#include <nx/network/system_socket.h>
#include <nx/utils/move_only_func.h>

namespace nx {
namespace vms {
namespace discovery {

/**
 * Multicasts module information and listen for such multicasts to discover VMS modules.
 */
class UdpMulticastFinder:
    public network::aio::BasicPollable
{
public:
    static const SocketAddress kMulticastEndpoint;
    static const std::chrono::milliseconds kUpdateInterfacesInterval;
    static const std::chrono::milliseconds kSendInterval;

    UdpMulticastFinder(network::aio::AbstractAioThread* thread = nullptr);
    void setMulticastEndpoint(SocketAddress endpoint);
    void setUpdateInterfacesInterval(std::chrono::milliseconds interval);
    void setSendInterval(std::chrono::milliseconds interval);

    /** Sets moduleInformation to multicast, starts multicast process if not running yet. */
    void multicastInformation(const QnModuleInformationWithAddresses& information);

    /** Listens to module information multicasts, executes handler on each message recieved. */
    void listen(nx::utils::MoveOnlyFunc<void(const QnModuleInformationWithAddresses& module)> handler);

    virtual void stopWhileInAioThread() override;
    void updateInterfaces();

private:
    typedef std::map<HostAddress, std::unique_ptr<network::UDPSocket>> Senders;

    std::unique_ptr<network::UDPSocket> makeSocket(const SocketAddress& endpoint);
    void joinMulticastGroup(const HostAddress& ip);
    void receiveModuleInformation();
    void sendModuleInformation(Senders::iterator senderIterator);

private:
    SocketAddress m_multicatEndpoint;
    std::chrono::milliseconds m_updateInterfacesInterval;
    std::chrono::milliseconds m_sendInterval;

    network::aio::Timer m_updateTimer;
    Buffer m_ownModuleInformation;
    Senders m_senders;

    Buffer m_inData;
    std::unique_ptr<network::UDPSocket> m_receiver;
    nx::utils::MoveOnlyFunc<void(const QnModuleInformationWithAddresses& module)> m_moduleHandler;
};

} // namespace discovery
} // namespace vms
} // namespace nx
