
#pragma once

#include <QtCore/QObject>

#include <statistics/statistics_fwd.h>
#include <utils/common/model_functions_fwd.h>

struct QnStatisticsSettings
{
    int limit;
    int storeDays;
    int minSendPeriodSecs;
    QnStringsSet filters;
    QString statisticsServerUrl;

    QnStatisticsSettings();
};

#define QnStatisticsSettings_Fields (limit)(storeDays)(minSendPeriodSecs)(filters)(statisticsServerUrl)
QN_FUSION_DECLARE_FUNCTIONS(QnStatisticsSettings, (json)(ubjson)(xml)(csv_record)(eq)(metatype))

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

signals:
    void settingsAvailableChanged();
};