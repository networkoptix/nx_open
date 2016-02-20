
#pragma once

#include <QtCore/QElapsedTimer>

#include <statistics/base/abstract_metric.h>

class QnTimeDurationMetric : public QnAbstractMetric
{
    typedef QnAbstractMetric base_type;

public:
    QnTimeDurationMetric(bool active = false);

    virtual ~QnTimeDurationMetric();

    void setCounterActive(bool isActive);

    bool isCounterActive() const;

public:
    void reset() override;

    QString value() const override;

    bool isSignificant() const override;

    qint64 duration() const;

private:
    QElapsedTimer m_counter;
    bool m_isActiveState;
    qint64 m_activeStateDurationMs;
};