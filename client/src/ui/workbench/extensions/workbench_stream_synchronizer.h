#ifndef QN_WORKBENCH_STREAM_SYNCHRONIZER_H
#define QN_WORKBENCH_STREAM_SYNCHRONIZER_H

#include <QObject>
#include <QWeakPointer>
#include "recording/time_period.h"

class QnCounter;
class QnWorkbenchDisplay;
class QnResourceWidget;
class QnArchiveSyncPlayWrapper;
class QnWorkbenchRenderWatcher;

class QnAbstractRenderer;

class QnWorkbenchStreamSynchronizer: public QObject {
    Q_OBJECT;
public:
    QnWorkbenchStreamSynchronizer(QnWorkbenchDisplay *display, QnWorkbenchRenderWatcher *renderWatcher, QObject *parent = NULL);

protected slots:
    void at_display_widgetAdded(QnResourceWidget *widget);
    void at_enable_sync(bool value);
    void at_display_widgetAboutToBeRemoved(QnResourceWidget *widget);
    void at_renderWatcher_displayingStateChanged(QnAbstractRenderer *renderer, bool displaying);
    //void at_playback_mask_changed(const QnTimePeriodList& playbackMask);

private:
    QnCounter *m_counter;
    QnArchiveSyncPlayWrapper *m_syncPlay;
    QWeakPointer<QnWorkbenchDisplay> m_display;
};

#endif // QN_WORKBENCH_STREAM_SYNCHRONIZER_H
