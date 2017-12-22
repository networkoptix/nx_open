#pragma once

#include <iostream>
#include <memory>

namespace nx {

namespace hpm { namespace api { class MediatorConnector; } }
namespace utils { class ArgumentParser; }

namespace network {

namespace aio { class AIOService; }
class AddressResolver;

namespace cloud {

class MediatorAddressPublisher;
class OutgoingTunnelPool;
class CloudConnectSettings;
namespace tcp { class ReverseConnectionPool; }

struct CloudConnectControllerImpl;

class NX_NETWORK_API CloudConnectController
{
public:
    CloudConnectController(
        aio::AIOService* aioService,
        AddressResolver* addressResolver);
    ~CloudConnectController();

    void applyArguments(const utils::ArgumentParser& arguments);

    hpm::api::MediatorConnector& mediatorConnector();
    MediatorAddressPublisher& addressPublisher();
    OutgoingTunnelPool& outgoingTunnelPool();
    CloudConnectSettings& settings();
    tcp::ReverseConnectionPool& tcpReversePool();

    /**
     * Deletes all objects and creates them.
     */
    void reinitialize();

    static void printArgumentsHelp(std::ostream* outputStream);

private:
    struct Settings
    {
        QString forcedMediatorUrl;
        bool isUdpHpDisabled = false;
        bool isOnlyCloudProxyEnabled = false;
    };

    std::unique_ptr<CloudConnectControllerImpl> m_impl;
    Settings m_settings;

    void loadSettings(const utils::ArgumentParser& arguments);
    void applySettings();
};

} // namespace cloud
} // namespace network
} // namespace nx
