#pragma once

#include <nx/utils/thread/mutex.h>
#include <statistics/base/abstract_metric.h>

class UdtInternetTrafficMetric: public QnAbstractMetric
{
public:
    UdtInternetTrafficMetric();
    virtual ~UdtInternetTrafficMetric() override;

    virtual bool isSignificant() const override;
    virtual QString value() const override;
    virtual void reset() override;

private:
    mutable QnMutex m_mutex;
    uint64_t m_readBytes = 0;
    uint64_t m_writeBytes = 0;
};

