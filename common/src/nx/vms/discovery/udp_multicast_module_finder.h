#pragma once

#include <network/module_information.h>
#include <nx/network/aio/timer.h>
#include <nx/network/system_socket.h>

namespace nx {
namespace vms {
namespace discovery {

/**
 * Multicasts module information and listen for such multicasts to discover VMS modules.
 */
class UdpMulticastModuleFinder:
    public network::aio::BasicPollable
{
public:
    UdpMulticastModuleFinder();

    /** Sets moduleInformation to multicast, starts multicast process if not running yet. */
    void multicastInformation(const QnModuleInformationWithAddresses& information);

    /** Listens to module information multicasts, executes handler on each message recieved. */
    void listen(utils::MoveOnlyFunc<void(const QnModuleInformationWithAddresses& module)> handler);

protected:
    virtual void stopWhileInAioThread() override;

private:
    void updateInterfaces();
    void makeInterface(const HostAddress& ip);

    class Interface
    {
    public:
        Interface(
            network::aio::AbstractAioThread* thread,
            const HostAddress& localIp,
            utils::MoveOnlyFunc<void()> errorHandler);

        void updateData(const Buffer* data);
        void readForever(utils::MoveOnlyFunc<void(const Buffer&)> handler);

    private:
        void sendData();
        void readData();
        void handleError(const char* stage, SystemError::ErrorCode code);

    private:
        network::UDPSocket m_socket;
        utils::MoveOnlyFunc<void()> m_errorHandler;
        const Buffer* m_outData;
        Buffer m_inData;
        utils::MoveOnlyFunc<void(const Buffer&)> m_newDataHandler;
    };

private:
    network::aio::Timer m_updateTimer;
    Buffer m_ownModuleInformation;
    utils::MoveOnlyFunc<void(const QnModuleInformationWithAddresses& module)> m_foundModuleHandler;
    std::map<HostAddress, std::unique_ptr<Interface>> m_interfaces;
};

} // namespace discovery
} // namespace vms
} // namespace nx
