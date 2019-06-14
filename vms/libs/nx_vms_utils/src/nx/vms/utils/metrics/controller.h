#pragma once

#include "resource_providers.h"

namespace nx::vms::utils::metrics {

class NX_VMS_UTILS_API Controller
{
public:
    void registerGroup(QString group, std::unique_ptr<AbstractResourceProvider> resourceProvider);
    void startMonitoring();

    api::metrics::SystemRules rules() const;
    void setRules(api::metrics::SystemRules rules);

    api::metrics::SystemManifest manifest(
        bool applyRules = true) const;

    api::metrics::SystemValues values(
        bool applyRules = true,
        std::optional<std::chrono::milliseconds> timeline = {}) const;

private:
    void loadGroupValuesUnlocked(
        std::map<QString /*id*/, api::metrics::ParameterGroupValues>* group,
        DataBase::Access dataBaseAccess,
        const std::vector<api::metrics::ParameterGroupManifest>& manifests,
        std::optional<std::chrono::milliseconds> timeLine) const;

    void applyRulesUnlocked(
        std::map<QString /*id*/, api::metrics::ParameterGroupValues>* group,
        DataBase::Access dataBaseAccess,
        const std::map<QString /*id*/, api::metrics::ParameterGroupRules>& rules) const;

    void applyRulesUnlocked(
        std::vector<api::metrics::ParameterGroupManifest>* group,
        const std::map<QString /*id*/, api::metrics::ParameterGroupRules>& rules) const;

private:
    // TODO: Should not be mutable as soon as DataBase supports separate read/write access.
    mutable DataBase m_dataBase;

    mutable nx::utils::Mutex m_mutex;
    api::metrics::SystemRules m_rules;
    std::map<QString, std::unique_ptr<AbstractResourceProvider>> m_resourceProviders;
};

} // namespace nx::vms::utils::metrics


