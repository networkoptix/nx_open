#pragma once

#include <iostream>
#include <memory>
#include <string>

#include <nx/kit/ini_config.h>

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

struct NX_NETWORK_API CloudConnectControllerImpl;

struct NX_NETWORK_API Ini:
    nx::kit::IniConfig
{
    Ini();

    NX_INI_STRING("", cloudHost, "Overridden Cloud Host");
    NX_INI_FLAG(0, disableCloudSockets, "Use plain TCP sockets instead of Cloud sockets");
};

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

    const Ini& ini() const;

    /**
     * Deletes all objects and creates them.
     */
    void reinitialize();

    static void printArgumentsHelp(std::ostream* outputStream);

private:
    struct Settings
    {
        std::string forcedMediatorUrl;
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
