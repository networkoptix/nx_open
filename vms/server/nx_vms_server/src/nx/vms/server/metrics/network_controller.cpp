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
    utils::metrics::Scope scope() const override { return utils::metrics::Scope::local; };

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
        utils::metrics::makeValueGroupProvider<Resource>(
            "info",
            utils::metrics::makeLocalValueProvider<Resource>(
                "name", [](const auto& r) { return Value(r.name); }
            ),
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
                "inBps", [](const auto&) { return Value(666); } // TODO: Implement.
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "outBps", [](const auto&) { return Value(777); } // TODO: Implement.
            )
        )
    );
}

} // namespace nx::vms::server::metrics



