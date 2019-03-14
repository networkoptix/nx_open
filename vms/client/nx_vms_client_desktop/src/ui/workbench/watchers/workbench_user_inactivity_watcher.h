#pragma once

#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtCore/QDateTime>
#include <QtCore/QPointer>

#include <QtWidgets/QWidget>

#include <ui/workbench/workbench_context_aware.h>

/**
 * The QnWorkbenchUserInactivityWatcher class implements the watcher of
 * the user activity. The user is considered as inactive when he idle
 * for a period specified by idleTimeoutMSecs() or the main window
 * is minimized for the same period.
 * When the user state changes the signal stateChanged() is emmited.
 */
class QnWorkbenchUserInactivityWatcher : public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
public:
    explicit QnWorkbenchUserInactivityWatcher(QObject *parent = 0);
    virtual ~QnWorkbenchUserInactivityWatcher();

    /** Time period in milliseconds since the latest user activity */
    quint64 idlePeriodMSecs() const;

    /** Current user activity state.
     *  @return true if idlePeriodMSecs() > idleTimeoutMSecs() */
    bool state() const;

    bool isEnabled() const;
    void setEnabled(bool enable);

    /** Time period in milliseconds which specifies an idle period after wich the user
     *  is considered as inactive. */
    quint64 idleTimeoutMSecs() const;
    void setIdleTimeoutMSecs(quint64 msecs);

    /** Set main window widget.
     *  This widget will be monitored for minimize/normalize events.
     *  If main window is minimized then idle time begins at the moment
     *  when it was minimized and all input events are ignored.
     */
    void setMainWindow(QWidget *widget);

    /** Event filter is installed in main window widget to
     *  handle minimize/normalize events.
     *  @see setMainWindow() */
    virtual bool eventFilter(QObject *object, QEvent *event);

signals:
    /** This signal is emmited when user activity state has been changed. */
    void stateChanged(bool userIsActive);

protected:
    void setState(bool userIsInactive);
    void checkInactivity();
    virtual void timerEvent(QTimerEvent *event);

private slots:
    /** Read timeout value from the application settings. */
    void updateTimeout();

private:
    int m_timerId;
    bool m_userIsInactive;
    quint32 m_idleTimeout;
    QPointer<QWidget> m_mainWindow;
    QDateTime m_mainWindowMinimizedTime;
};
