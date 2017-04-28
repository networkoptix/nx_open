#pragma once

#include <QtCore/QObject>

#include <statistics/statistics_settings.h>

class QnAbstractStatisticsSettingsLoader : public QObject
{
    Q_OBJECT

    typedef QObject base_type;

    Q_PROPERTY(bool settingsAvailable READ settingsAvailable NOTIFY settingsAvailableChanged)

public:
    QnAbstractStatisticsSettingsLoader(QObject *parent = nullptr);

    virtual ~QnAbstractStatisticsSettingsLoader();

    virtual bool settingsAvailable() = 0;

    virtual QnStatisticsSettings settings() = 0;

    virtual void updateSettings() = 0;

signals:
    void settingsAvailableChanged();
};