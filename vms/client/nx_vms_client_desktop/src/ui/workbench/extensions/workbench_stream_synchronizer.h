// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMetaType>
#include <QtCore/QObject>
#include <QtCore/QSet>

#include <libavcodec/avcodec.h>

#include <core/resource/resource_fwd.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/counter.h>
#include <nx/vms/client/desktop/window_context_aware.h>

class QnResourceWidget;
class QnResourceDisplay;
typedef QSharedPointer<QnResourceDisplay> QnResourceDisplayPtr;
class QnMediaResourceWidget;
class QnArchiveSyncPlayWrapper;
class QnWorkbenchRenderWatcher;

struct QnStreamSynchronizationState
{
    QnStreamSynchronizationState();
    QnStreamSynchronizationState(bool started, qint64 time, qreal speed);
    static QnStreamSynchronizationState live();

    bool isSyncOn;
    qint64 timeUs;
    qreal speed;
};
QN_FUSION_DECLARE_FUNCTIONS(QnStreamSynchronizationState, (json)(metatype))

/**
 * This class manages the necessary machinery for synchronized playback of
 * cameras on the scene.
 */
class QnWorkbenchStreamSynchronizer:
    public QObject,
    public nx::vms::client::desktop::WindowContextAware
{
    Q_OBJECT

public:
    QnWorkbenchStreamSynchronizer(
        nx::vms::client::desktop::WindowContext* windowContext, QObject* parent = nullptr);

    /**
     * Stops this stream synchronizer.
     * When stopped, UTC video streams on the scene are not synchronized.
     */
    void stop();

    /**
     * Starts this stream synchronizer.
     *
     * \param timeUSec                  Starting time for all synchronizable streams, in
     * microseconds since epoch. \param speed                     Playback speed for all
     * synchronizable streams.
     */
    void start(qint64 timeUSec, float speed);

    /**
     * \returns                         Whether this stream synchronizer is running.
     */
    bool isRunning() const;

    QnStreamSynchronizationState state() const;

    void setState(const QnStreamSynchronizationState& state);

    void setState(QnResourceWidget* widget, bool useWidgetPausedState = false);

    /**
     * \returns                         Whether enabling or disabling the synchronizer will have
     * any effect. That is, whether there are any items to synchronize.
     */
    bool isEffective() const;

signals:
    void runningChanged();
    void effectiveChanged();

protected slots:
    void at_display_widgetAdded(QnResourceWidget* widget);
    void at_display_widgetAboutToBeRemoved(QnResourceWidget* widget);
    void at_renderWatcher_displayChanged(const QnResourceDisplayPtr& display);

    void at_workbench_currentLayoutChanged();

    void at_resource_flagsChanged(const QnResourcePtr& resource);

private:
    /** Counter that is used to track the number of references to syncplay
     * instance. When it reaches zero, syncplay is destroyed. */
    nx::utils::CounterWithSignal* m_counter = nullptr;

    /** Syncplay instance that performs the actual stream synchronization. */
    QnArchiveSyncPlayWrapper* m_syncPlay = nullptr;

    /** Display state watcher. */
    QnWorkbenchRenderWatcher* m_watcher = nullptr;

    QSet<QnMediaResourceWidget*> m_syncedWidgets;
    QSet<QnMediaResourceWidget*> m_queuedWidgets;

    void handleWidget(QnMediaResourceWidget* widget);
};
