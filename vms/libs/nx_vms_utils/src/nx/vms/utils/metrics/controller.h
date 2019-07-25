#pragma once

#include "resource_providers.h"

namespace nx::vms::utils::metrics {

class NX_VMS_UTILS_API Controller
{
public:
    Controller(nx::utils::MoveOnlyFunc<uint64_t()> currentSecsSinceEpoch = nullptr);

    void registerGroup(QString group, std::unique_ptr<AbstractResourceProvider> resourceProvider);
    void startMonitoring();

    api::metrics::SystemRules rules() const;
    void setRules(api::metrics::SystemRules rules);

    enum RequestFlag
    { 
        none = 0,
        applyRules = (1 << 0), 
        includeRemote = (1 << 1)
    };
    using RequestFlags = QFlags<RequestFlag>;

    api::metrics::SystemManifest manifest(RequestFlags flags) const;
    api::metrics::SystemValues values(
        RequestFlags flags, std::optional<std::chrono::milliseconds> timeline = {}) const;

private:
    void applyRulesUnlocked(
        std::map<QString /*id*/, api::metrics::ParameterGroupValues>* group,
        DataBase::Reader reader,
        const std::map<QString /*id*/, api::metrics::ParameterGroupRules>& rules) const;

    void applyRulesUnlocked(
        std::vector<api::metrics::ParameterGroupManifest>* group,
        const std::map<QString /*id*/, api::metrics::ParameterGroupRules>& rules) const;

private:
    const nx::utils::MoveOnlyFunc<uint64_t()> m_currentSecsSinceEpoch;

    // TODO: Should not be mutable as soon as DataBase supports separate read/write access.
    mutable DataBase m_dataBase;

    mutable nx::utils::Mutex m_mutex;
    api::metrics::SystemRules m_rules;
    std::map<QString, std::unique_ptr<AbstractResourceProvider>> m_resourceProviders;
};

} // namespace nx::vms::utils::metrics


