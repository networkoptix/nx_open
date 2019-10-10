#include "network_controller.h"

#include "helpers.h"

namespace nx::vms::server::metrics {

NetworkController::NetworkController(const QnUuid& serverId):
    utils::metrics::ResourceControllerImpl<nx::network::QnInterfaceAndAddr>(
        "networkInterfaces", makeProviders()),
    m_serverId(serverId.toSimpleString())
{
}

void NetworkController::start()
{
    // TODO: Add monitor for add/remove.
    for (auto iface: nx::network::getAllIPv4Interfaces())
        add(std::move(iface), m_serverId + "_" + iface.name, utils::metrics::Scope::local);
}

utils::metrics::ValueGroupProviders<NetworkController::Resource> NetworkController::makeProviders()
{
    return nx::utils::make_container<utils::metrics::ValueGroupProviders<Resource>>(
        utils::metrics::makeValueGroupProvider<Resource>(
            "_",
            utils::metrics::makeLocalValueProvider<Resource>(
                "name", [](const auto& r) { return Value(r.name); }
            )
        ),
        utils::metrics::makeValueGroupProvider<Resource>(
            "info",
            utils::metrics::makeLocalValueProvider<Resource>(
                "server", [this](const auto&) { return Value(m_serverId); }
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "ip", [](const auto& r) { return Value(r.address.toString()); }
            )
        ),
        utils::metrics::makeValueGroupProvider<Resource>(
            "rates",
            utils::metrics::makeLocalValueProvider<Resource>(
                "inBps", [](const auto&) { return Value(0); } // TODO: Implement.
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "outBps", [](const auto&) { return Value(0); } // TODO: Implement.
            )
        )
    );
}

} // namespace nx::vms::server::metrics



