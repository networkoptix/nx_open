// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/


#pragma once

#include <QtCore/QObject>

#include <utils/common/connective.h>
#include <statistics/base/time_duration_metric.h>

class QnWorkbenchDisplay;
class QnResourceWidget;

class CameraFullscreenMetric : public Connective<QObject>
    , public QnTimeDurationMetric
{
    typedef Connective<QObject> base_type;

public:
    CameraFullscreenMetric(QnWorkbenchDisplay *display);

    virtual ~CameraFullscreenMetric();

private:
    typedef QPointer<QnResourceWidget> WidgetPointer;

    WidgetPointer m_currentFullscreenWidget;
};