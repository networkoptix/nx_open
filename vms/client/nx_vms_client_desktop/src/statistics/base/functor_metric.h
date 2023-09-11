// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <statistics/base/abstract_metric.h>
#include <statistics/base/base_fwd.h>

class QnFunctorMetric: public QnAbstractMetric
{
    typedef QnAbstractMetric base_type;

public:
    typedef std::function<QString ()> ValueFunctor;
    typedef std::function<bool ()> IsSignificantFunctor;

    static QnAbstractMetricPtr create(const ValueFunctor &valueFunctor
        , const IsSignificantFunctor &isSignificantFunctor = IsSignificantFunctor());

    QnFunctorMetric(const ValueFunctor &valueFunctor
        , const IsSignificantFunctor &isSignificantFunctor = IsSignificantFunctor());

    virtual ~QnFunctorMetric();

    bool isSignificant() const override;

    QString value() const override;

    void reset() override;

private:
    const ValueFunctor m_valueFunctor;
    const IsSignificantFunctor m_isSignificantFunctor;
};
