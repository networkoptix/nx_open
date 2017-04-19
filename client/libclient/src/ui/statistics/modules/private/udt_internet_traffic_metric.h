#pragma once

#include <nx/utils/thread/mutex.h>
#include <statistics/base/abstract_metric.h>

class UdtInternetTrafficMetric: public QnAbstractMetric
{
public:
    virtual bool isSignificant() const override;
    virtual QString value() const override;
    virtual void reset() override;
};

