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
    QString name() const override { return this->resource.name; }
    QString parent() const override { return m_serverId; }

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
    for (const QnInterfaceAndAddr& interface : nx::network::getAllIPv4Interfaces())
        add(std::make_unique<InterfaceDescription>(interface, m_serverId));
}

utils::metrics::ValueGroupProviders<NetworkController::Resource> NetworkController::makeProviders()
{
    return nx::utils::make_container<utils::metrics::ValueGroupProviders<Resource>>(
        std::make_unique<utils::metrics::ValueGroupProvider<Resource>>(
            api::metrics::Label{
                "info", "Info"
            },
            std::make_unique<utils::metrics::ValueProvider<Resource>>(
                api::metrics::ValueManifest{"ip", "IP", "table&panel", ""},
                [](const auto& r) { return Value(r.address.toString()); }
            )
        ),
        std::make_unique<utils::metrics::ValueGroupProvider<Resource>>(
            api::metrics::Label{
                "rates", "I/O rates"
            },
            std::make_unique<utils::metrics::ValueProvider<Resource>>(
                api::metrics::ValueManifest{"in", "IN rate", "table&panel", "bps"},
                [](const auto&) { return Value(); } // TODO: Implement.
            ),
            std::make_unique<utils::metrics::ValueProvider<Resource>>(
                api::metrics::ValueManifest{"out", "OUT rate", "table&panel", "bps"},
                [](const auto&) { return Value(); } // TODO: Implement.
            )
        )
    );
}

} // namespace nx::vms::server::metrics



