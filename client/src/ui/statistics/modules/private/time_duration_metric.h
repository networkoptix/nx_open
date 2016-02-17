
#pragma once

#include <QtCore/QElapsedTimer>

#include <ui/statistics/modules/private/abstract_single_metric.h>

class TimeDurationMetric : public AbstractSingleMetric
{
    typedef AbstractSingleMetric base_type;

public:
    TimeDurationMetric(bool active = false);

    virtual ~TimeDurationMetric();

    void activateCounter(bool isActive);

    bool isActive() const;

public:
    void reset() override;

    QString value() const override;

    bool significant() const override;

    qint64 duration() const;

private:
    QElapsedTimer m_counter;
    bool m_isActiveState;
    qint64 m_activeStateDurationMs;
};