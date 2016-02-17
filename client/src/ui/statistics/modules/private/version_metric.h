
#pragma once

#include <statistics/base/abstract_metric.h>

class VersionMetric : public QnAbstractMetric
{
    typedef QnAbstractMetric base_type;
public:
    VersionMetric();

    virtual ~VersionMetric();

    bool significant() const override;

    QString value() const override;

    void reset() override;
};