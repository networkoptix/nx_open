// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "certificate_statistics_module.h"

#include <nx/utils/log/log.h>

QnStatisticValuesHash QnCertificateStatisticsModule::values() const
{
    QnStatisticValuesHash result;
    for (auto scenario: m_values.keys())
    {
        auto values = m_values[scenario];
        for (auto alias: values.keys())
        {
            result.insert(nx::format("%1_%2", scenario, alias), QString::number(values[alias]));
        }
    }
    return result;
}

void QnCertificateStatisticsModule::reset()
{
    m_values.clear();
}

void QnCertificateStatisticsModule::registerClick(const QString& alias)
{
    if (!m_scenario)
        return;

    ++(m_values[*m_scenario][alias]);
}

std::shared_ptr<QnCertificateStatisticsModule::ScenarioGuard>
    QnCertificateStatisticsModule::beginScenario(Scenario scenario)
{
    return std::make_shared<ScenarioGuard>(this, scenario);
}

void QnCertificateStatisticsModule::setScenario(std::optional<Scenario> scenario)
{
    m_scenario = scenario;
}

QnCertificateStatisticsModule::ScenarioGuard::ScenarioGuard(
    QnCertificateStatisticsModule* owner, Scenario scenario)
    :
    m_owner(owner)
{
    NX_CRITICAL(m_owner);

    if (NX_ASSERT(!m_owner->m_scenario.has_value(),
        "Failed to start '%1' scenario, '%2' is already running", scenario, *m_owner->m_scenario))
    {
        NX_VERBOSE(this, "Starting '%1' scenario", scenario);
        m_owner->setScenario(scenario);
    }
    else
    {
        m_owner.clear(); //< Clear pointer to disable destructor logic.
    }
}

QnCertificateStatisticsModule::ScenarioGuard::~ScenarioGuard()
{
    if (m_owner)
        m_owner->setScenario(std::nullopt);
}
