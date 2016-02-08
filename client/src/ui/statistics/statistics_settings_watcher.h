
#pragma once

#include <QtCore/QObject>

#include <utils/common/connective.h>
#include <api/server_rest_connection_fwd.h>
#include <statistics/base_statistics_settings_loader.h>

class QnStatisticsSettingsWatcher : public Connective<QnBaseStatisticsSettingsLoader>
{
    Q_OBJECT

    typedef Connective<QnBaseStatisticsSettingsLoader> base_type;

public:
    QnStatisticsSettingsWatcher(QObject *parent = nullptr);

    virtual ~QnStatisticsSettingsWatcher();

    //

    bool settingsAvailable() override;

    QnStatisticsSettings settings() override;

private:
    void updateSettings();

private:
    typedef QScopedPointer<QTimer> TimerPtr;
    typedef QScopedPointer<QnStatisticsSettings> SettingsPtr;

    SettingsPtr m_settings;
    TimerPtr m_updateTimer;

    rest::QnConnectionPtr m_connection;
    rest::Handle m_restHandle;
};