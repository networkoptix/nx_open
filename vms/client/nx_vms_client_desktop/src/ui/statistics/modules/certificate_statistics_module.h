// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMap>

#include <nx/vms/client/desktop/system_logon/data/logon_data.h>
#include <statistics/abstract_statistics_module.h>

class QnCertificateStatisticsModule: public QnAbstractStatisticsModule
{
    Q_OBJECT
    using base_type = QnAbstractStatisticsModule;

public:
    using Scenario = nx::vms::client::desktop::ConnectScenario;

    class ScenarioGuard
    {
    public:
        ScenarioGuard(QnCertificateStatisticsModule* owner, Scenario scenario);
        ~ScenarioGuard();
    private:
        QPointer<QnCertificateStatisticsModule> m_owner;
    };

    friend class ScenarioGuard;

public:
    using QnAbstractStatisticsModule::QnAbstractStatisticsModule;
    virtual ~QnCertificateStatisticsModule() = default;
    virtual QnStatisticValuesHash values() const;
    virtual void reset();

    void registerClick(const QString& alias);
    std::shared_ptr<ScenarioGuard> beginScenario(Scenario scenario);
    void resetScenario();

private:
    void setScenario(std::optional<Scenario> scenario);

private:
    using Values = QHash<QString, int>;

    std::optional<Scenario> m_scenario;
    QMap<Scenario, Values> m_values;
};

using QnStatisticsScenarioGuard = QnCertificateStatisticsModule::ScenarioGuard;
