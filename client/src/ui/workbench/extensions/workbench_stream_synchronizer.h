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

/**
 * This class manages the necessary machinery for synchronized playback of
 * cameras on the scene.
 */
class QnWorkbenchStreamSynchronizer: public QObject {
    Q_OBJECT;
public:
    QnWorkbenchStreamSynchronizer(QnWorkbenchDisplay *display, QnWorkbenchRenderWatcher *renderWatcher, QObject *parent = NULL);

    void setEnabled(bool enabled);
    bool isEnabled() const;

protected slots:
    void at_display_widgetAdded(QnResourceWidget *widget);
    void at_display_widgetAboutToBeRemoved(QnResourceWidget *widget);
    void at_renderWatcher_displayingStateChanged(QnAbstractRenderer *renderer, bool displaying);

private:
    QnCounter *m_counter;
    QnArchiveSyncPlayWrapper *m_syncPlay;
    QWeakPointer<QnWorkbenchDisplay> m_display;
};

#endif // QN_WORKBENCH_STREAM_SYNCHRONIZER_H
