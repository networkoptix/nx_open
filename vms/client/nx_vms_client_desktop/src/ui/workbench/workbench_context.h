// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <typeinfo>

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <nx/core/watermark/watermark.h>
#include <nx/vms/client/core/common/utils/common_module_aware.h>
#include <nx/vms/client/desktop/ui/actions/action_fwd.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <utils/common/instance_storage.h>

struct QnStartupParameters;

class QnWorkbench;
class QnWorkbenchSynchronizer;
class QnWorkbenchAccessController;
class QnWorkbenchDisplay;
class QnWorkbenchNavigator;
class QnWorkbenchLayoutWatcher;

namespace nx::vms::client::desktop {

class ContextCurrentUserWatcher;
class MainWindow;
class ResourceTreeSettings;
class IntercomManager;

namespace joystick { class Manager; }

} // namespace nx::vms::client::desktop

/**
 * This is a class that ties together all objects comprising the global visual scene state.
 * Its instance exists through all application's lifetime.
 */
class QnWorkbenchContext:
    public QObject,
    public QnInstanceStorage,
    public nx::vms::client::core::CommonModuleAware
{
    Q_OBJECT
    using base_type = QObject;
    Q_PROPERTY(QString userId READ userId NOTIFY userIdChanged)
    Q_PROPERTY(QWidget* mainWindow READ mainWindowWidget NOTIFY mainWindowChanged)

public:
    QnWorkbenchContext(QnWorkbenchAccessController* accessController, QObject *parent = nullptr);

    virtual ~QnWorkbenchContext();

    QnWorkbench* workbench() const;
    nx::vms::client::desktop::ui::action::Manager* menu() const;

    QnWorkbenchAccessController* accessController() const;
    void setAccessController(QnWorkbenchAccessController* value);

    QnWorkbenchDisplay* display() const;
    QnWorkbenchNavigator* navigator() const;
    nx::vms::client::desktop::joystick::Manager* joystickManager() const;

    nx::vms::client::desktop::MainWindow* mainWindow() const;
    QWidget* mainWindowWidget() const;
    void setMainWindow(nx::vms::client::desktop::MainWindow* mainWindow);

    nx::vms::client::desktop::ResourceTreeSettings* resourceTreeSettings() const;

    nx::core::Watermark watermark() const;

    QAction *action(const nx::vms::client::desktop::ui::action::IDType id) const;

    QnUserResourcePtr user() const;
    void setUserName(const QString &userName);

    /** User ID as QString for QML usage. */
    QString userId() const;

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
    void userChanged(const QnUserResourcePtr &user);

    /** Property change notification signal, emitted together with userChanged(). */
    void userIdChanged();

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
    QScopedPointer<nx::vms::client::desktop::ui::action::Manager> m_menu;
    QScopedPointer<QnWorkbenchDisplay> m_display;
    QScopedPointer<QnWorkbenchSynchronizer> m_synchronizer;
    QScopedPointer<QnWorkbenchNavigator> m_navigator;
    QScopedPointer<nx::vms::client::desktop::ResourceTreeSettings> m_resourceTreeSettings;
    std::unique_ptr<nx::vms::client::desktop::joystick::Manager> m_joystickManager;

    // Have to be moved in nx::vms::client::desktop::SystemContext when it will be possible.
    std::unique_ptr<nx::vms::client::desktop::IntercomManager> m_intercomManager;

    QPointer<nx::vms::client::desktop::MainWindow> m_mainWindow;

    QnWorkbenchAccessController* m_accessController;
    nx::vms::client::desktop::ContextCurrentUserWatcher* m_userWatcher;
    QnWorkbenchLayoutWatcher *m_layoutWatcher;

    bool m_closingDown;
};
