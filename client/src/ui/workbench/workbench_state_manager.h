#ifndef WORKBENCH_STATE_MANAGER_H
#define WORKBENCH_STATE_MANAGER_H

#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchStateDelegate: public QnWorkbenchContextAware {
public:
    QnWorkbenchStateDelegate(QObject *parent = NULL);
    ~QnWorkbenchStateDelegate();

    virtual bool tryClose(bool force) = 0;
    virtual void forcedUpdate() = 0;
};

template <typename T>
class QnBasicWorkbenchStateDelegate: public QnWorkbenchStateDelegate {
public:
    QnBasicWorkbenchStateDelegate(T* owner):
        QnWorkbenchStateDelegate(owner),
        m_owner(owner)
    {}

    virtual bool tryClose(bool force) override {
        return m_owner->tryClose(force);
    }

    virtual void forcedUpdate() override {
        m_owner->forcedUpdate();
    }

private:
    T* m_owner;
}; 

class QnWorkbenchStateManager: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT
public:
    QnWorkbenchStateManager(QObject *parent = NULL);

    /** Forcibly update all delegates. 
     *  Should be called in case of full state re-read (reconnect, merge systems, etc). 
     **/
    void forcedUpdate();

    bool tryClose(bool force);
private:
    friend class QnWorkbenchStateDelegate;

    void registerDelegate(QnWorkbenchStateDelegate *d);
    void unregisterDelegate(QnWorkbenchStateDelegate *d);

private:
    QList<QnWorkbenchStateDelegate*> m_delegates;
};


#endif //WORKBENCH_STATE_MANAGER_H