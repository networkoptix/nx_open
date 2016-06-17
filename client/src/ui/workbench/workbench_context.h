#pragma once

#include <typeinfo>

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <utils/common/instance_storage.h>
#include <core/resource/resource_fwd.h>
#include <ui/actions/actions.h>

struct QnStartupParameters;

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
class QnControlsStatisticsModule;

/**
 * This is a class that ties together all objects comprising the global visual scene state
 */
class QnWorkbenchContext: public QObject, public QnInstanceStorage
{
    Q_OBJECT
public:
    QnWorkbenchContext(QObject *parent = NULL);

    virtual ~QnWorkbenchContext();

    QnWorkbench* workbench() const;
    QnWorkbenchLayoutSnapshotManager* snapshotManager() const;
    QnActionManager* menu() const
    {
        return m_menu.data();
    }

    QnWorkbenchAccessController* accessController() const;
    QnWorkbenchDisplay* display() const;
    QnWorkbenchNavigator* navigator() const;
    QnControlsStatisticsModule* statisticsModule() const;

    QWidget* mainWindow() const;
    void setMainWindow(QWidget* mainWindow);

    QAction *action(const QnActions::IDType id) const;

    QnUserResourcePtr user() const;
    void setUserName(const QString &userName);

    /** Check if application is closing down. Replaces QApplication::closingDown(). */
    bool closingDown() const;
    void setClosingDown(bool value);

    /** Process startup parameters and call related actions. Returns false if something goes critically wrong. */
    bool handleStartupParameters(const QnStartupParameters& startupParams);

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
    void initWorkarounds();

private:
    QScopedPointer<QnWorkbench> m_workbench;
    QScopedPointer<QnWorkbenchSynchronizer> m_synchronizer;
    QScopedPointer<QnWorkbenchLayoutSnapshotManager> m_snapshotManager;
    QScopedPointer<QnWorkbenchAccessController> m_accessController;
    QScopedPointer<QnActionManager> m_menu;
    QScopedPointer<QnWorkbenchDisplay> m_display;
    QScopedPointer<QnWorkbenchNavigator> m_navigator;
    QScopedPointer<QnControlsStatisticsModule> m_statisticsModule;

    QPointer<QWidget> m_mainWindow;

    QnWorkbenchUserWatcher *m_userWatcher;
    QnWorkbenchLayoutWatcher *m_layoutWatcher;

    bool m_closingDown;
};
