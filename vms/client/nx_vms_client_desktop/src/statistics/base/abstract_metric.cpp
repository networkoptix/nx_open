// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/


#include "abstract_metric.h"

QnAbstractMetric::QnAbstractMetric()
{}

QnAbstractMetric::~QnAbstractMetric()
{}

bool QnAbstractMetric::isSignificant() const
{
    return true;
}
