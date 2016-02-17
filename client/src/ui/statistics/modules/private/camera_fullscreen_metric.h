
#pragma once

#include <QtCore/QObject>

#include <utils/common/connective.h>
#include <ui/statistics/modules/private/time_duration_metric.h>

class QnWorkbenchDisplay;
class QnResourceWidget;

class CameraFullscreenMetric : public Connective<QObject>
    , public TimeDurationMetric
{
    typedef Connective<QObject> base_type;

public:
    CameraFullscreenMetric(QnWorkbenchDisplay *display);

    virtual ~CameraFullscreenMetric();

private:
    typedef QPointer<QnResourceWidget> WidgetPointer;

    WidgetPointer m_currentFullscreenWidget;
};