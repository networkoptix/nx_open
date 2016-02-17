
#pragma once

#include <ui/statistics/modules/private/abstract_single_metric.h>

class VersionMetric : public AbstractSingleMetric
{
    typedef AbstractSingleMetric base_type;
public:
    VersionMetric();

    virtual ~VersionMetric();

    bool significant() const override;

    QString value() const override;

    void reset() override;
};