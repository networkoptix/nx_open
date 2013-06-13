#ifndef WORKBENCH_USER_EMAIL_WATCHER_H
#define WORKBENCH_USER_EMAIL_WATCHER_H

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchUserEmailWatcher : public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
public:
    explicit QnWorkbenchUserEmailWatcher(QObject *parent = 0);
    virtual ~QnWorkbenchUserEmailWatcher();

signals:
    void userEmailValidityChanged(const QnUserResourcePtr &user, bool isValid);

private slots:
    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);

    void at_user_emailChanged(const QnUserResourcePtr &user);

private:
    QHash<QnUserResourcePtr, bool> m_emailValidByUser;
};

#endif // WORKBENCH_USER_EMAIL_WATCHER_H
