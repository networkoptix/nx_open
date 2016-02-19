
#pragma once

#include <QtCore/QObject>

#include <utils/common/connective.h>
#include <api/server_rest_connection_fwd.h>
#include <statistics/abstract_statistics_settings_loader.h>

class QnStatisticsSettingsWatcher : public Connective<QnAbstractStatisticsSettingsLoader>
{
    Q_OBJECT

    typedef Connective<QnAbstractStatisticsSettingsLoader> base_type;

public:
    QnStatisticsSettingsWatcher(QObject *parent = nullptr);

    virtual ~QnStatisticsSettingsWatcher();

    //

    bool settingsAvailable() override;

    QnStatisticsSettings settings() override;

private:
    void updateSettings();

    void updateSettingsImpl(int delayMs);

private:
    void setSettings(const QnStatisticsSettings &settings);

    void resetSettings();

private:
    typedef QScopedPointer<QnStatisticsSettings> SettingsPtr;
    typedef QScopedPointer<QTimer> TimerPtr;
    SettingsPtr m_settings;
    TimerPtr m_updateTimer;

    rest::Handle m_handle;
};