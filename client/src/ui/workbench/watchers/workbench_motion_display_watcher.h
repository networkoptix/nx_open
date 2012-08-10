#ifndef QN_WORKBENCH_MOTION_DISPLAY_WATCHER_H
#define QN_WORKBENCH_MOTION_DISPLAY_WATCHER_H

#include <QObject>
#include <QWeakPointer>
#include <QSet>

class QnWorkbenchDisplay;
class QnResourceWidget;

class QnWorkbenchMotionDisplayWatcher: public QObject {
    Q_OBJECT;
public:
    QnWorkbenchMotionDisplayWatcher(QnWorkbenchDisplay *display, QObject *parent = NULL);

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
    QWeakPointer<QnWorkbenchDisplay> m_display;
    QSet<QnResourceWidget *> m_widgets;
};

#endif // QN_WORKBENCH_MOTION_DISPLAY_WATCHER_H
