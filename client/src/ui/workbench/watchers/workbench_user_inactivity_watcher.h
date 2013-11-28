#ifndef WORKBENCH_USER_INACTIVITY_WATCHER_H
#define WORKBENCH_USER_INACTIVITY_WATCHER_H

#include <QtCore/QObject>
#include <QtCore/QTimer>

#include <ui/workbench/workbench_context_aware.h>

/**
 * The QnWorkbenchUserInactivityWatcher class implements the watcher of
 * the user activity. The user is considered as inactive when he idle
 * for a period specified by idleTimeout().
 * When the user state changes the signal stateChanged() is emmited.
 */
class QnWorkbenchUserInactivityWatcher : public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
public:
    explicit QnWorkbenchUserInactivityWatcher(QObject *parent = 0);
    virtual ~QnWorkbenchUserInactivityWatcher();

    /** Time period in milliseconds since the latest user activity */
    quint32 idlePeriod() const;

    /** Current user activity state.
     *  \note It's true when idlePeriod() > idleTimeout() */
    bool state() const;

    bool isEnabled() const;
    void setEnabled(bool enable);

    /** Time period in milliseconds which specifies an idle period after wich the user
     *  is considered as inactive. */
    quint32 idleTimeout() const;
    void setIdleTimeout(quint32 timeout);

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
};

#endif // WORKBENCH_USER_INACTIVITY_WATCHER_H
