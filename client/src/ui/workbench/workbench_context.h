#ifndef QN_WORKBENCH_CONTEXT_H
#define QN_WORKBENCH_CONTEXT_H

#include <typeinfo>

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <ui/actions/actions.h>

class QAction;

class QnResourcePool;
class QnWorkbench;
class QnWorkbenchSynchronizer;
class QnWorkbenchLayoutSnapshotManager;
class QnWorkbenchAccessController;
class QnWorkbenchDisplay;
class QnWorkbenchNavigator;
class QnWorkbenchUserWatcher;
class QnWorkbenchLayoutWatcher;
class QnActionManager;

/**
 * This is a class that ties together all objects comprising the global state 
 * and serves as an application context.
 */
class QnWorkbenchContext: public QObject {
    Q_OBJECT
public:
    QnWorkbenchContext(QnResourcePool *resourcePool, QObject *parent = NULL);

    virtual ~QnWorkbenchContext();

    QnResourcePool *resourcePool() const {
        return m_resourcePool;
    }

    QnWorkbench *workbench() const {
        return m_workbench.data();
    }

    QnWorkbenchSynchronizer *synchronizer() const {
        return m_synchronizer.data();
    }

    QnWorkbenchLayoutSnapshotManager *snapshotManager() const {
        return m_snapshotManager.data();
    }

    QnActionManager *menu() const {
        return m_menu.data();
    }

    QnWorkbenchAccessController *accessController() const {
        return m_accessController.data();
    }

    QnWorkbenchDisplay *display() const {
        return m_display.data();
    }

    QnWorkbenchNavigator *navigator() const {
        return m_navigator.data();
    }

    QWidget *mainWindow() const {
        return m_mainWindow.data();
    }

    void setMainWindow(QWidget *mainWindow) {
        m_mainWindow = mainWindow;
    }

    QAction *action(const Qn::ActionId id) const;

    QnUserResourcePtr user() const;

    void setUserName(const QString &userName);

    template<class T>
    T *instance() {
        QByteArray key(typeid(T).name());
        QObject *&result = m_instanceByTypeName[key];
        if(!result) {
            result = new T(this);
            m_instances.push_back(result);
        }
        return static_cast<T *>(result);
    }

signals:
    /**
     * This signal is emitted whenever the user that is currently logged in changes.
     * 
     * \param user                      New user that was logged in. May be null.
     */
    void userChanged(const QnUserResourcePtr &user); // TODO: #Elric remove user parameter

    /**
     * This signal is emitted when this workbench context is about to be destroyed,
     * but before its state is altered in any way by the destructor.
     */
    void aboutToBeDestroyed();

protected slots:
    void at_resourcePool_aboutToBeDestroyed();

private:
    QnResourcePool *m_resourcePool;
    QScopedPointer<QnWorkbench> m_workbench;
    QScopedPointer<QnWorkbenchSynchronizer> m_synchronizer;
    QScopedPointer<QnWorkbenchLayoutSnapshotManager> m_snapshotManager;
    QScopedPointer<QnWorkbenchAccessController> m_accessController;
    QScopedPointer<QnActionManager> m_menu;
    QScopedPointer<QnWorkbenchDisplay> m_display;
    QScopedPointer<QnWorkbenchNavigator> m_navigator;
    
    QWeakPointer<QWidget> m_mainWindow;

    QnWorkbenchUserWatcher *m_userWatcher;
    QnWorkbenchLayoutWatcher *m_layoutWatcher;
    QHash<QByteArray, QObject *> m_instanceByTypeName; // TODO: #Elric use std::type_index
    QList<QObject *> m_instances;
};


#endif // QN_WORKBENCH_CONTEXT_H
