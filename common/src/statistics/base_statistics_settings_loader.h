
#pragma once

#include <QtCore/QObject>

#include <statistics/types.h>
#include <utils/common/model_functions_fwd.h>

struct QnStatisticsSettings
{
    int limit;
    int storeDays;
    QnStringsSet filters;

    QnStatisticsSettings();
};

#define QnStatisticsSettings_Fields (limit)(storeDays)(filters)
QN_FUSION_DECLARE_FUNCTIONS(QnStatisticsSettings, (json)(ubjson)(xml)(csv_record)(eq)(metatype))

class QnBaseStatisticsSettingsLoader : public QObject
{
    Q_OBJECT

    typedef QObject base_type;

    Q_PROPERTY(bool settingsAvailable READ settingsAvailable NOTIFY settingsAvailableChanged)

public:
    QnBaseStatisticsSettingsLoader(QObject *parent = nullptr);

    virtual ~QnBaseStatisticsSettingsLoader();

    virtual bool settingsAvailable() = 0;

    virtual QnStatisticsSettings settings() = 0;

signals:
    void settingsAvailableChanged();
};