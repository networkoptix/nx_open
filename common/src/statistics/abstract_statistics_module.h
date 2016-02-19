
#pragma once

#include <QtCore/QObject>

#include <utils/common/connective.h>
#include <statistics/statistics_fwd.h>

class QnAbstractStatisticsModule : public Connective<QObject>
{
    Q_OBJECT

    typedef Connective<QObject> base_type;

public:
    QnAbstractStatisticsModule(QObject *parent = nullptr);

    virtual ~QnAbstractStatisticsModule();

    virtual QnMetricsHash metrics() const = 0;

    virtual void resetMetrics() = 0;
};
