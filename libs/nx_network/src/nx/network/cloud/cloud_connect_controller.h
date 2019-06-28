#pragma once

#include <iostream>
#include <memory>
#include <string>

#include <QtCore/QString>

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

struct NX_NETWORK_API CloudConnectControllerImpl;

class NX_NETWORK_API CloudConnectController
{
public:
    CloudConnectController(
        const QString& customCloudHost,
        aio::AIOService* aioService,
        AddressResolver* addressResolver);
    virtual ~CloudConnectController();

    void applyArguments(const utils::ArgumentParser& arguments);

    const QString& cloudHost() const;
    hpm::api::MediatorConnector& mediatorConnector();
    MediatorAddressPublisher& addressPublisher();
    OutgoingTunnelPool& outgoingTunnelPool();
    CloudConnectSettings& settings();

    /**
     * Deletes all objects and creates them.
     */
    void reinitialize();

    static void printArgumentsHelp(std::ostream* outputStream);

private:
    std::unique_ptr<CloudConnectControllerImpl> m_impl;

    void loadSettings(const utils::ArgumentParser& arguments);
    void applySettings();
};

} // namespace cloud
} // namespace network
} // namespace nx
