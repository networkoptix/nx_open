#pragma once

#include <typeinfo>

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <client_core/connection_context_aware.h>

#include <utils/common/instance_storage.h>
#include <core/resource/resource_fwd.h>
#include <nx/client/desktop/ui/actions/action_fwd.h>
#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/vms/utils/system_uri.h>

struct QnStartupParameters;

class QnWorkbench;
class QnWorkbenchSynchronizer;
class QnWorkbenchLayoutSnapshotManager;
class QnWorkbenchAccessController;
class QnWorkbenchDisplay;
class QnWorkbenchNavigator;
class QnWorkbenchUserWatcher;
class QnWorkbenchLayoutWatcher;
class QnControlsStatisticsModule;

namespace nx {
namespace client {
namespace desktop {
namespace ui {

class MainWindow;

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx

/**
 * This is a class that ties together all objects comprising the global visual scene state
 */
class QnWorkbenchContext: public QObject, public QnInstanceStorage, public QnConnectionContextAware
{
    Q_OBJECT
    using base_type = QObject;
public:
    QnWorkbenchContext(QnWorkbenchAccessController* accessController, QObject *parent = NULL);

    virtual ~QnWorkbenchContext();

    QnWorkbench* workbench() const;
    QnWorkbenchLayoutSnapshotManager* snapshotManager() const;
    nx::client::desktop::ui::action::Manager* menu() const;

    QnWorkbenchAccessController* accessController() const;
    void setAccessController(QnWorkbenchAccessController* value);

    QnWorkbenchDisplay* display() const;
    QnWorkbenchNavigator* navigator() const;
    QnControlsStatisticsModule* statisticsModule() const;

    nx::client::desktop::ui::MainWindow* mainWindow() const;
    void setMainWindow(nx::client::desktop::ui::MainWindow* mainWindow);

    QAction *action(const nx::client::desktop::ui::action::IDType id) const;

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

    bool connectUsingCustomUri(const nx::vms::utils::SystemUri& uri);

    bool connectUsingCommandLineAuth(const QnStartupParameters& startupParams);

private:
    QScopedPointer<QnWorkbench> m_workbench;
    QScopedPointer<QnWorkbenchSynchronizer> m_synchronizer;
    QScopedPointer<QnWorkbenchLayoutSnapshotManager> m_snapshotManager;
    QScopedPointer<nx::client::desktop::ui::action::Manager> m_menu;
    QScopedPointer<QnWorkbenchDisplay> m_display;
    QScopedPointer<QnWorkbenchNavigator> m_navigator;
    QScopedPointer<QnControlsStatisticsModule> m_statisticsModule;

    QPointer<nx::client::desktop::ui::MainWindow> m_mainWindow;

    QnWorkbenchAccessController* m_accessController;
    QnWorkbenchUserWatcher *m_userWatcher;
    QnWorkbenchLayoutWatcher *m_layoutWatcher;

    bool m_closingDown;
};
