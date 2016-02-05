
#pragma once

#include <QtCore/QObject>

#include <utils/common/connective.h>
#include <statistics/base_statistics_settings_loader.h>

class QnStatisticsSettingsWatcher : public Connective<QnBaseStatisticsSettingsLoader>
{
    Q_OBJECT

    typedef Connective<QnBaseStatisticsSettingsLoader> base_type;

public:
    QnStatisticsSettingsWatcher(QObject *parent = nullptr);

    virtual ~QnStatisticsSettingsWatcher();

    QnStatisticsSettings settings() const override;

private:
    void updateSettings();

private:
    typedef QScopedPointer<QTimer> TimerPtr;

    QnStatisticsSettings m_settings;
    TimerPtr m_updateTimer;

};