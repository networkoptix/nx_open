#pragma once

#include "providers.h"

namespace nx::vms::server::metrics {

class ResourceGroupController
{
public:
    ResourceGroupController(std::unique_ptr<AbstractResourceProvider> resourceProvider);

    QString id() const;
    void startMonitoring();

    api::metrics::ResourceRules rules();
    void setRules(api::metrics::ResourceRules rules);

    api::metrics::ResourceManifest manifest();
    api::metrics::ResourceGroupValues rawValues();
    api::metrics::ResourceGroupValues values();

private:
    api::metrics::ResourceGroupValues rawValuesUnlocked();

    void loadGroupValuesUnlocked(
        std::map<QString /*id*/, api::metrics::ParameterGroupValues>* group,
        const DataBaseKey& base,
        const std::vector<api::metrics::ParameterGroupManifest>& manifests);

    void applyRulesUnlocked(
        std::map<QString /*id*/, api::metrics::ParameterGroupValues>* group,
        const DataBaseKey& baseKey,
        const std::map<QString /*id*/, api::metrics::ParameterGroupRules>& rules);

    void applyRulesUnlocked(
        std::vector<api::metrics::ParameterGroupManifest>* group,
        const std::map<QString /*id*/, api::metrics::ParameterGroupRules>& rules);

private:
    const std::unique_ptr<AbstractResourceProvider> m_resourceProvider;
    const std::unique_ptr<ParameterGroupProvider> m_parameterProviders;
    const api::metrics::ParameterGroupManifest m_parameterManifest;

    DataBase m_dataBase;

    mutable nx::utils::Mutex m_mutex;
    api::metrics::ResourceRules m_rules;
    std::map<QnResourcePtr, std::unique_ptr<AbstractParameterProvider::Monitor>> m_parameterMonitors;
};

} // namespace nx::vms::server::metrics
