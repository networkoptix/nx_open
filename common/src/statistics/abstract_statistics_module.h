
#pragma once

#include <QtCore/QObject>

#include <statistics/statistics_fwd.h>

class QnAbstractStatisticsModule : public QObject
{
    Q_OBJECT

    typedef QObject base_type;

public:
    QnAbstractStatisticsModule(QObject *parent = nullptr);

    virtual ~QnAbstractStatisticsModule();

    virtual QnMetricsHash metrics() const = 0;

    virtual void resetMetrics() = 0;
};
