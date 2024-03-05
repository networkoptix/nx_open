// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <iostream>
#include <memory>
#include <string>

namespace nx { class ArgumentParser; }
namespace nx::hpm::api { class MediatorConnector; }
namespace nx::network { class AddressResolver; }
namespace nx::network::aio { class AIOService; }

namespace nx::network::cloud {

class MediatorAddressPublisher;
class OutgoingTunnelPool;
class CloudConnectSettings;

struct NX_NETWORK_API CloudConnectControllerImpl;

class NX_NETWORK_API CloudConnectController
{
public:
    CloudConnectController(
        const std::string& customCloudHost,
        aio::AIOService* aioService,
        AddressResolver* addressResolver);
    virtual ~CloudConnectController();

    void applyArguments(const ArgumentParser& arguments);

    const std::string& cloudHost() const;
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

    void loadSettings(const ArgumentParser& arguments);
    void applySettings();
};

} // namespace nx::network::cloud
