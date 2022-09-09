// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <statistics/base/time_duration_metric.h>

namespace nx::vms::client::desktop { class Workbench; }

class PreviewSearchDurationMetric: public QObject, public QnTimeDurationMetric
{
    typedef QObject base_type;

public:
    PreviewSearchDurationMetric(nx::vms::client::desktop::Workbench* workbench);

    virtual ~PreviewSearchDurationMetric();
};
