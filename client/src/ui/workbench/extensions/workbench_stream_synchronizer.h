#ifndef QN_WORKBENCH_STREAM_SYNCHRONIZER_H
#define QN_WORKBENCH_STREAM_SYNCHRONIZER_H

#include <libavcodec/avcodec.h>

#include <QtCore/QObject>

#include <QtCore/QSet>
#include <QtCore/QMetaType>

#include <core/resource/resource_fwd.h>

#include <utils/common/model_functions_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

class QnCounter;
class QnResourceWidget;
class QnResourceDisplay;
class QnMediaResourceWidget;
class QnArchiveSyncPlayWrapper;
class QnWorkbenchRenderWatcher;


struct QnStreamSynchronizationState {
    QnStreamSynchronizationState(): started(false), time(AV_NOPTS_VALUE), speed(0.0) {}
    QnStreamSynchronizationState(bool started, qint64 time, qreal speed): started(started), time(time), speed(speed) {}

    bool started;
    qint64 time;
    qreal speed;
};
QN_FUSION_DECLARE_FUNCTIONS(QnStreamSynchronizationState, (json)(metatype))

/**
 * This class manages the necessary machinery for synchronized playback of
 * cameras on the scene.
 */
class QnWorkbenchStreamSynchronizer: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT;
public:
    QnWorkbenchStreamSynchronizer(QObject *parent = NULL);

    /**
     * Stops this stream synchronizer. 
     * When stopped, UTC video streams on the scene are not synchronized.
     */
    void stop();

    /**
     * Starts this stream synchronizer. 
     * 
     * \param timeUSec                  Starting time for all synchronizable streams, in microseconds since epoch.
     * \param speed                     Playback speed for all synchronizable streams.
     */
    void start(qint64 timeUSec, float speed);

    /**
     * \returns                         Whether this stream synchronizer is running.
     */
    bool isRunning() const;


    QnStreamSynchronizationState state() const;

    void setState(const QnStreamSynchronizationState &state);

    void setState(QnResourceWidget *widget);


    /**
     * \returns                         Whether enabling or disabling the synchronizer will have any effect.
     *                                  That is, whether there are any items to synchronize.
     */
    bool isEffective() const;

signals:
    void runningChanged();
    void effectiveChanged();

protected slots:
    void at_display_widgetAdded(QnResourceWidget *widget);
    void at_display_widgetAboutToBeRemoved(QnResourceWidget *widget);
    void at_renderWatcher_displayChanged(QnResourceDisplay *display);

    void at_workbench_currentLayoutChanged();

    void at_resource_flagsChanged(const QnResourcePtr &resource);

private:
    /** Number of widgets that can be synchronized. */
    int m_widgetCount;

    /** Counter that is used to track the number of references to syncplay
     * instance. When it reaches zero, syncplay is destroyed. */
    QnCounter *m_counter;

    /** Syncplay instance that performs the actual stream synchronization. */
    QnArchiveSyncPlayWrapper *m_syncPlay;

    /** Display state watcher. */
    QnWorkbenchRenderWatcher *m_watcher;

    QSet<QnMediaResourceWidget *> m_queuedWidgets;
};

#endif // QN_WORKBENCH_STREAM_SYNCHRONIZER_H
