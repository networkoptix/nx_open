#ifndef WORKBENCH_STATE_MANAGER_H
#define WORKBENCH_STATE_MANAGER_H

#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchStateDelegate {
public:
    virtual bool tryClose(bool force) = 0;
};

class QnWorkbenchStateManager: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT
public:

    QnWorkbenchStateManager(QObject *parent = NULL);

    bool tryClose(bool force);

    void registerDelegate(QnWorkbenchStateDelegate *d);
    void unregisterDelegate(QnWorkbenchStateDelegate *d);

private:
    QList<QnWorkbenchStateDelegate*> m_delegates;

};


#endif //WORKBENCH_STATE_MANAGER_H