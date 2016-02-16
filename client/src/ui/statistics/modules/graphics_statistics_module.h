
#pragma once

#include <statistics/abstract_statistics_module.h>

// + Time when the window is active
// Time when the window is in fullscreen mode

// Time when video is live (per camera??? what with desync layouts?)
// Time when video is in archive
// Time when video is paused
// Time when the camera is on fullscreen (any camera)
// Time when control panels are visible (per panel)
// Time when panels are unpinned

class QnWorkbenchContext;

class QnGraphicsStatisticsModule : public QnAbstractStatisticsModule
{
    Q_OBJECT

    typedef QnAbstractStatisticsModule base_type;

public:
    QnGraphicsStatisticsModule(QObject *parent);

    virtual ~QnGraphicsStatisticsModule();

    virtual QnMetricsHash metrics() const;

    virtual void resetMetrics();

    void setContext(QnWorkbenchContext *context);

private:
    typedef QPointer<QnWorkbenchContext> ContextPtr;
    ContextPtr m_context;


};
