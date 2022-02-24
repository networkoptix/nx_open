// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/


#include "functor_metric.h"

QnAbstractMetricPtr QnFunctorMetric::create(const ValueFunctor &valueFunctor
    , const IsSignificantFunctor &isSignificantFunctor)
{
    return QnAbstractMetricPtr(new QnFunctorMetric(
        valueFunctor, isSignificantFunctor));
}

QnFunctorMetric::QnFunctorMetric(const ValueFunctor &valueFunctor
    , const IsSignificantFunctor &isSignificantFunctor)
    : base_type()
    , m_valueFunctor(valueFunctor)
    , m_isSignificantFunctor(isSignificantFunctor)
{

}

QnFunctorMetric::~QnFunctorMetric()
{}

bool QnFunctorMetric::isSignificant() const
{
    return (!m_isSignificantFunctor || m_isSignificantFunctor());
}

QString QnFunctorMetric::value() const
{
    return m_valueFunctor();
}

void QnFunctorMetric::reset()
{}
