// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/


#pragma once

#include <QtCore/QObject>

#include <utils/common/connective.h>
#include <statistics/base/statistics_values_provider.h>

class QnAbstractStatisticsModule : public Connective<QObject>
    , public QnStatisticsValuesProvider
{
    Q_OBJECT

    typedef Connective<QObject> base_type;

public:
    QnAbstractStatisticsModule(QObject *parent = nullptr)
        : base_type(parent)
        , QnStatisticsValuesProvider()
    {}

    virtual ~QnAbstractStatisticsModule() {}
};
