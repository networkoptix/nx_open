#ifndef WORKBENCH_STATE_MANAGER_H
#define WORKBENCH_STATE_MANAGER_H

#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchStateDelegate {
public:
    virtual bool tryClose(bool force) = 0;
};

template <typename T>
class QnBasicWorkbenchStateDelegate: public QnWorkbenchStateDelegate {
public:
    QnBasicWorkbenchStateDelegate(T* owner):
        m_owner(owner)
    {}

    virtual bool tryClose(bool force) override {
        return m_owner->tryClose(force);
    }

private:
    T* m_owner;
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