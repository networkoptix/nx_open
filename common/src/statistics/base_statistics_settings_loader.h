
#pragma once

#include <QtCore/QObject>

#include <statistics/types.h>

struct QnStatisticsSettings
{
    int limit;
    int storeDays;
    QnStringsSet filters;

    QnStatisticsSettings();
};

#define QnStatisticsSettings_Fields (limit)(storeDays)(filters)
QN_FUSION_DECLARE_FUNCTIONS(QnStatisticsSettings, (json)(eq))

class QnBaseStatisticsSettingsLoader : public QObject
{
    typedef QObject base_type;

public:
    QnBaseStatisticsSettingsLoader(QObject *parent = nullptr);

    virtual ~QnBaseStatisticsSettingsLoader();

    virtual QnStatisticsSettings settings() const = 0;
};