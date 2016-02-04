
#pragma once

#include <QtCore/QObject>

#include <ui/statistics/types.h>
#include <utils/common/connective.h>

struct QnStatisticsSettings
{
    int limit;
    int storeDays;
    QnStringsSet filters;

    QnStatisticsSettings();
};

#define QnStatisticsSettings_Fields (limit)(storeDays)(filters)
QN_FUSION_DECLARE_FUNCTIONS(QnStatisticsSettings, (json)(eq))

class QnStatisticsSettingsWatcher : public Connective<QObject>
{
    Q_OBJECT

    typedef Connective<QObject> base_type;

public:
    QnStatisticsSettingsWatcher(QObject *parent = nullptr);

    virtual ~QnStatisticsSettingsWatcher();

    QnStatisticsSettings settings() const;

private:
    void updateSettings();

private:
    typedef QScopedPointer<QTimer> TimerPtr;

    QnStatisticsSettings m_settings;
    TimerPtr m_updateTimer;

};