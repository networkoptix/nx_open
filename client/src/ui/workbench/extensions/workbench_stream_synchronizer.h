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

    /**
     * Enables or disables this stream synchronizer. 
     * When disabled, video streams of cameras on the scene are not synchronized.
     * 
     * \param enabled                   Whether this stream synchronizer is to be enabled.
     */
    void setEnabled(bool enabled);

    /**
     * \returns                         Whether this stream synchronizer is enabled.
     */
    bool isEnabled() const;

    /**
     * \returns                         Whether enabling or disabling the synchronizer will have any effect.
     *                                  That is, whether there are any items to synchronize.
     */
    bool isEffective() const;
    
signals:
    void effectiveChanged();

protected slots:
    void at_display_widgetAdded(QnResourceWidget *widget);
    void at_display_widgetAboutToBeRemoved(QnResourceWidget *widget);
    void at_renderWatcher_displayingStateChanged(QnAbstractRenderer *renderer, bool displaying);

private:
    /** Number of widgets that can be synchronized. */
    int m_widgetCount;

    /** Counter that is used to track the number of references to syncplay
     * instance. When it reaches zero, syncplay is destroyed. */
    QnCounter *m_counter;

    /** Syncplay instance that performs the actual stream synchronization. */
    QnArchiveSyncPlayWrapper *m_syncPlay;

    /** Workbench display that this stream synchronizer was created for. */
    QWeakPointer<QnWorkbenchDisplay> m_display;
};

#endif // QN_WORKBENCH_STREAM_SYNCHRONIZER_H
