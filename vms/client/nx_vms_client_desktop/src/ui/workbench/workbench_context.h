#pragma once

#include <typeinfo>

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <client_core/connection_context_aware.h>

#include <utils/common/instance_storage.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/ui/actions/action_fwd.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <nx/vms/utils/system_uri.h>
#include <nx/core/watermark/watermark.h>

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

namespace nx::vms::client::desktop { class MainWindow; }

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
    nx::vms::client::desktop::ui::action::Manager* menu() const;

    QnWorkbenchAccessController* accessController() const;
    void setAccessController(QnWorkbenchAccessController* value);

    QnWorkbenchDisplay* display() const;
    QnWorkbenchNavigator* navigator() const;
    QnControlsStatisticsModule* statisticsModule() const;

    nx::vms::client::desktop::MainWindow* mainWindow() const;
    QWidget* mainWindowWidget() const;
    void setMainWindow(nx::vms::client::desktop::MainWindow* mainWindow);

    nx::core::Watermark watermark() const;

    QAction *action(const nx::vms::client::desktop::ui::action::IDType id) const;

    QnUserResourcePtr user() const;
    void setUserName(const QString &userName);

    /** Check if application is closing down. Replaces QApplication::closingDown(). */
    bool closingDown() const;
    void setClosingDown(bool value);

    /**
     * Process startup parameters and call related actions.
     */
    void handleStartupParameters(const QnStartupParameters& startupParams);

    /** Whether the scene is visible in the main window. */
    bool isWorkbenchVisible() const;

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
    QScopedPointer<nx::vms::client::desktop::ui::action::Manager> m_menu;
    QScopedPointer<QnWorkbenchDisplay> m_display;
    QScopedPointer<QnWorkbenchNavigator> m_navigator;
    QScopedPointer<QnControlsStatisticsModule> m_statisticsModule;

    QPointer<nx::vms::client::desktop::MainWindow> m_mainWindow;

    QnWorkbenchAccessController* m_accessController;
    QnWorkbenchUserWatcher *m_userWatcher;
    QnWorkbenchLayoutWatcher *m_layoutWatcher;

    bool m_closingDown;
};
