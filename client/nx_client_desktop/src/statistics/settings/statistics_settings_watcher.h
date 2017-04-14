#pragma once

#include <QtCore/QObject>

#include <client_core/connection_context_aware.h>

#include <utils/common/connective.h>
#include <api/server_rest_connection_fwd.h>
#include <statistics/abstract_statistics_settings_loader.h>

class QnStatisticsSettingsWatcher:
    public Connective<QnAbstractStatisticsSettingsLoader>,
    public QnConnectionContextAware
{
    Q_OBJECT

    typedef Connective<QnAbstractStatisticsSettingsLoader> base_type;

public:
    QnStatisticsSettingsWatcher(QObject *parent = nullptr);

    virtual ~QnStatisticsSettingsWatcher();

    //

    bool settingsAvailable() override;

    QnStatisticsSettings settings() override;

    void updateSettings() override;

private:
    void updateSettingsImpl(int delayMs);

private:
    void setSettings(const QnStatisticsSettings &settings);

    void resetSettings();

private:
    typedef QScopedPointer<QnStatisticsSettings> SettingsPtr;

    SettingsPtr m_settings;

    rest::Handle m_handle;
};