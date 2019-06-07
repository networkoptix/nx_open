#pragma once

#include "providers.h"

namespace nx::vms::server::metrics {

class Controller
{
public:
    void registerGroup(std::unique_ptr<AbstractResourceProvider> resourceProvider);
    void startMonitoring();

    api::metrics::SystemRules rules();
    void setRules(api::metrics::SystemRules rules);

    api::metrics::SystemManifest manifest();
    api::metrics::SystemValues rawValues();
    api::metrics::SystemValues values();

private:
    api::metrics::SystemValues rawValuesUnlocked();

    void loadGroupValuesUnlocked(
        std::map<QString /*id*/, api::metrics::ParameterGroupValues>* group,
        DataBase::Access base,
        const std::vector<api::metrics::ParameterGroupManifest>& manifests);

    void applyRulesUnlocked(
        std::map<QString /*id*/, api::metrics::ParameterGroupValues>* group,
        DataBase::Access baseKey,
        const std::map<QString /*id*/, api::metrics::ParameterGroupRules>& rules);

    void applyRulesUnlocked(
        std::vector<api::metrics::ParameterGroupManifest>* group,
        const std::map<QString /*id*/, api::metrics::ParameterGroupRules>& rules);

private:
    struct ResourceGroup
    {
        std::unique_ptr<AbstractResourceProvider> resourceProvider;
        std::unique_ptr<ParameterGroupProvider> parameterProviders;
        api::metrics::ParameterGroupManifest parameterManifest;
        std::map<QnResourcePtr, std::unique_ptr<AbstractParameterProvider::Monitor>>
            parameterMonitors;
    };

    DataBase m_dataBase;
    mutable nx::utils::Mutex m_mutex;
    api::metrics::SystemRules m_rules;
    std::map<QString, ResourceGroup> m_resourceGroups;
};

} // namespace nx::vms::server::metrics
