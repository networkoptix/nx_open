#ifndef WORKBENCH_USER_EMAIL_WATCHER_H
#define WORKBENCH_USER_EMAIL_WATCHER_H

#include <QtCore/QObject>
#include <QHash>

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

class QnWorkbenchUserEmailWatcher : public Connective<QObject>, public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef Connective<QObject> base_type;
public:
    explicit QnWorkbenchUserEmailWatcher(QObject *parent = 0);
    virtual ~QnWorkbenchUserEmailWatcher();

    void forceCheck(const QnUserResourcePtr &user);
    void forceCheckAll();

signals:
    void userEmailValidityChanged(const QnUserResourcePtr &user, bool isValid);

private slots:
    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);

    void at_user_emailChanged(const QnResourcePtr &resource);

private:
    QHash<QnUserResourcePtr, bool> m_emailValidByUser;
};

#endif // WORKBENCH_USER_EMAIL_WATCHER_H
