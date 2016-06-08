#pragma once

#include <typeinfo>

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <utils/common/instance_storage.h>
#include <core/resource/resource_fwd.h>
#include <ui/actions/actions.h>

class QAction;

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
class QnWorkbenchContext: public QObject, public QnInstanceStorage
{
    Q_OBJECT
public:
    QnWorkbenchContext(QObject *parent = NULL);

    virtual ~QnWorkbenchContext();

    QnWorkbench *workbench() const
    {
        return m_workbench.data();
    }

    QnWorkbenchLayoutSnapshotManager *snapshotManager() const
    {
        return m_snapshotManager.data();
    }

    QnActionManager *menu() const
    {
        return m_menu.data();
    }

    QnWorkbenchAccessController *accessController() const
    {
        return m_accessController.data();
    }

    QnWorkbenchDisplay *display() const
    {
        return m_display.data();
    }

    QnWorkbenchNavigator *navigator() const
    {
        return m_navigator.data();
    }

    QWidget *mainWindow() const
    {
        return m_mainWindow.data();
    }

    void setMainWindow(QWidget *mainWindow);

    QAction *action(const QnActions::IDType id) const;

    QnUserResourcePtr user() const;

    void setUserName(const QString &userName);

    /** Check if application is closing down. Replaces QApplication::closingDown(). */
    bool closingDown() const;
    void setClosingDown(bool value);

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

    void mainWindowChanged();

private:
    QScopedPointer<QnWorkbench> m_workbench;
    QScopedPointer<QnWorkbenchSynchronizer> m_synchronizer;
    QScopedPointer<QnWorkbenchLayoutSnapshotManager> m_snapshotManager;
    QScopedPointer<QnWorkbenchAccessController> m_accessController;
    QScopedPointer<QnActionManager> m_menu;
    QScopedPointer<QnWorkbenchDisplay> m_display;
    QScopedPointer<QnWorkbenchNavigator> m_navigator;

    QPointer<QWidget> m_mainWindow;

    QnWorkbenchUserWatcher *m_userWatcher;
    QnWorkbenchLayoutWatcher *m_layoutWatcher;

    bool m_closingDown;
};
