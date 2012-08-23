#ifndef QN_WORKBENCH_MOTION_DISPLAY_WATCHER_H
#define QN_WORKBENCH_MOTION_DISPLAY_WATCHER_H

#include <QtCore/QObject>
#include <QtCore/QWeakPointer>
#include <QtCore/QSet>

#include <ui/workbench/workbench_context_aware.h>

class QnResourceWidget;

/**
 * This class tracks whether there are any widgets on the scene with motion grid
 * displayed, and provides the necessary getters and notifiers to access this state.
 */
class QnWorkbenchMotionDisplayWatcher: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT;
public:
    QnWorkbenchMotionDisplayWatcher(QObject *parent = NULL);
    virtual ~QnWorkbenchMotionDisplayWatcher();

    /**
     * \returns                         Whether there is at least one widget on the scene that
     *                                  has motion grid displayed.
     */
    bool isMotionGridDisplayed() const {
        return !m_widgets.empty();
    }

signals:
    /**
     * This signal is emitted whenever motion grid becomes displayed on at least
     * one item on the scene.
     */
    void motionGridShown();

    /**
     * This signal is emitted whenever motion grid becomes no longer displayed on
     * any of the items on the scene.
     */
    void motionGridHidden();

protected slots:
    void at_display_widgetAdded(QnResourceWidget *widget);
    void at_display_widgetAboutToBeRemoved(QnResourceWidget *widget);
    void at_widget_displayFlagsChanged(QnResourceWidget *widget, int displayFlags);
    void at_widget_displayFlagsChanged();

private:
    QSet<QnResourceWidget *> m_widgets;
};

#endif // QN_WORKBENCH_MOTION_DISPLAY_WATCHER_H
