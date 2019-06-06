#include "providers.h"

namespace nx::vms::server::metrics {

ParameterGroupProvider::ParameterGroupProvider(
    QString id, QString name, ParameterProviders providers)
:
    m_id(std::move(id)),
    m_name(std::move(name)),
    m_providers(std::move(providers))
{
}

api::metrics::ParameterGroupManifest ParameterGroupProvider::manifest()
{
    auto manifest = api::metrics::makeParameterGroupManifest(m_id, m_name);
    for (const auto& provider: m_providers)
        manifest.group.push_back(provider->manifest());

    return manifest;
}

namespace {

struct GroupMonitor: public AbstractParameterProvider::Monitor
{
    std::map<QString /*id*/, std::unique_ptr<AbstractParameterProvider::Monitor>> monitors;

    virtual void start(DataBaseInserter inserter) override
    {
        for (const auto& [id, monitor]: monitors)
            monitor->start(inserter.subValue(id));
    }
};

} // namespace

std::unique_ptr<AbstractParameterProvider::Monitor> ParameterGroupProvider::monitor(
    QnResourcePtr resource)
{
    auto groupMonitor = std::make_unique<GroupMonitor>();
    for (const auto& provider: m_providers)
    {
        const auto id = provider->manifest().id;
        groupMonitor->monitors[id] = provider->monitor(resource);
    }

    return groupMonitor;
}

} // namespace nx::vms::server::metrics

