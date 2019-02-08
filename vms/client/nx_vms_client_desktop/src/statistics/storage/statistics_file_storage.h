#pragma once

#include <QtCore/QDir>

#include <statistics/abstract_statistics_storage.h>

class QnStatisticsFileStorage : public QnAbstractStatisticsStorage
{
    typedef QnAbstractStatisticsStorage base_type;
public:
    explicit QnStatisticsFileStorage();

    virtual ~QnStatisticsFileStorage();

private:
    void storeMetrics(const QnStatisticValuesHash &metrics) override;

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

    int m_fileCountLimit;
    int m_storeDaysCount;
};
