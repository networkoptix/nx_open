#include "network_controller.h"

#include "helpers.h"

namespace nx::vms::server::metrics {

namespace {

class InterfaceDescription:
    public utils::metrics::ResourceDescription<nx::network::QnInterfaceAndAddr>
{
public:
    InterfaceDescription(nx::network::QnInterfaceAndAddr resource, QString serverId):
        utils::metrics::ResourceDescription<nx::network::QnInterfaceAndAddr>(std::move(resource)),
        m_serverId(std::move(serverId))
    {
    }

    QString id() const override { return m_serverId + "_" + this->resource.name; };

private:
    const QString m_serverId;
};

} // namespace

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
        add(std::make_unique<InterfaceDescription>(std::move(iface), m_serverId));
}

utils::metrics::ValueGroupProviders<NetworkController::Resource> NetworkController::makeProviders()
{
    return nx::utils::make_container<utils::metrics::ValueGroupProviders<Resource>>(
        std::make_unique<utils::metrics::ValueGroupProvider<Resource>>(
            "info",
            std::make_unique<utils::metrics::ValueProvider<Resource>>(
                "name", [](const auto& r) { return Value(r.name); }
            ),
            std::make_unique<utils::metrics::ValueProvider<Resource>>(
                "server", [this](const auto&) { return Value(m_serverId); }
            ),
            std::make_unique<utils::metrics::ValueProvider<Resource>>(
                "ip", [](const auto& r) { return Value(r.address.toString()); }
            )
        ),
        std::make_unique<utils::metrics::ValueGroupProvider<Resource>>(
            "rates",
            std::make_unique<utils::metrics::ValueProvider<Resource>>(
                "inBps", [](const auto&) { return Value(666); } // TODO: Implement.
            ),
            std::make_unique<utils::metrics::ValueProvider<Resource>>(
                "outBps", [](const auto&) { return Value(777); } // TODO: Implement.
            )
        )
    );
}

} // namespace nx::vms::server::metrics



