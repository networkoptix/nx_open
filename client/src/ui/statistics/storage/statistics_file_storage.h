
#pragma once

#include <ui/statistics/base_statistics_storage.h>

class QnStatisticsFileStorage : public QnBaseStatisticsStorage
{
    typedef QnBaseStatisticsStorage base_type;
public:
    explicit QnStatisticsFileStorage(QObject *parent = nullptr);

    virtual ~QnStatisticsFileStorage();

private:
    void storeMetrics(const QnMetricsHash &metrics) override;

    QnMetricHashesList getMetricsList(qint64 startTimeMs, int limit) const override;

    //

    bool saveCustomSettings(const QString &name
        , const QByteArray &settings) override;

    QByteArray getCustomSettings(const QString &name) const override;

    void removeCustomSettings(const QString &name) override;

private:
    void removeOutdatedFiles();

private:
    QDir m_statisticsDirectory;
    QDir m_customSettingsDirectory;

    int m_limit;
    int m_storeDaysCount;
};