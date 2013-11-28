#ifndef WORKBENCH_USER_INACTIVITY_WATCHER_H
#define WORKBENCH_USER_INACTIVITY_WATCHER_H

#include <QtCore/QObject>
#include <QtCore/QTimer>

#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchUserInactivityWatcher : public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
public:
    explicit QnWorkbenchUserInactivityWatcher(QObject *parent = 0);
    virtual ~QnWorkbenchUserInactivityWatcher();

    quint32 idlePeriod() const;
    bool isUserInactive() const;

    bool isEnabled() const;
    void setEnabled(bool enable);

    quint32 idleTimeout() const;
    void setIdleTimeout(quint32 timeout);

signals:
    void userInactivityStatusChanged(bool userIsActive);

protected:
    void setUserInactive(bool userIsInactive);
    void checkInactivity();
    virtual void timerEvent(QTimerEvent *);

private slots:
    void updateTimeout();

private:
    int m_timerId;
    bool m_userIsInactive;
    quint32 m_idleTimeout;
};

#endif // WORKBENCH_USER_INACTIVITY_WATCHER_H
