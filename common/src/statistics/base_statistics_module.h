
#pragma once

#include <QtCore/QObject>

#include <statistics/types.h>

class QnBaseStatisticsModule : public QObject
{
    Q_OBJECT

    typedef QObject base_type;

public:
    QnBaseStatisticsModule(QObject *parent = nullptr);

    virtual ~QnBaseStatisticsModule();

    virtual QnMetricsHash metrics() const = 0;

    virtual void resetMetrics() = 0;
};
