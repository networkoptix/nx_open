// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <typeinfo>

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <nx/core/watermark/watermark.h>
#include <nx/vms/client/desktop/system_context_aware.h>
#include <nx/vms/client/desktop/ui/actions/action_fwd.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <utils/common/instance_storage.h>

struct QnStartupParameters;
class QnWorkbenchDisplay;
class QnWorkbenchNavigator;
class QnWorkbenchLayoutWatcher;

namespace nx::vms::client::core { class UserWatcher; }

namespace nx::vms::client::desktop {

class MainWindow;
class ResourceTreeSettings;
class SettingsDialogManager;
class IntercomManager;
class Workbench;

namespace joystick { class Manager; }

} // namespace nx::vms::client::desktop

/**
 * This is a class that ties together all objects comprising the global visual scene state.
 * Its instance exists through all application's lifetime.
 */
class QnWorkbenchContext:
    public QObject,
    public QnInstanceStorage,
    public nx::vms::client::desktop::SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;
    Q_PROPERTY(QString userId READ userId NOTIFY userIdChanged)
    Q_PROPERTY(QWidget* mainWindow READ mainWindowWidget CONSTANT)
    Q_PROPERTY(nx::vms::client::desktop::SystemContext* systemContext READ systemContext CONSTANT)

public:
    QnWorkbenchContext(
        nx::vms::client::desktop::SystemContext* systemContext,
        QObject* parent = nullptr);
    virtual ~QnWorkbenchContext();

    nx::vms::client::desktop::Workbench* workbench() const;
    nx::vms::client::desktop::ui::action::Manager* menu() const;

    QnWorkbenchDisplay* display() const;
    QnWorkbenchNavigator* navigator() const;
    nx::vms::client::desktop::joystick::Manager* joystickManager() const;
    nx::vms::client::desktop::SettingsDialogManager* settingsDialogManager() const;

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

private:
    void initWorkarounds();

private:
    QScopedPointer<nx::vms::client::desktop::Workbench> m_workbench;
    QScopedPointer<nx::vms::client::desktop::ui::action::Manager> m_menu;
    QScopedPointer<QnWorkbenchDisplay> m_display;
    QScopedPointer<QnWorkbenchNavigator> m_navigator;
    QScopedPointer<nx::vms::client::desktop::ResourceTreeSettings> m_resourceTreeSettings;
    std::unique_ptr<nx::vms::client::desktop::joystick::Manager> m_joystickManager;
    std::unique_ptr<nx::vms::client::desktop::SettingsDialogManager> m_settingsDialogManager;

    // Have to be moved in nx::vms::client::desktop::SystemContext when it will be possible.
    std::unique_ptr<nx::vms::client::desktop::IntercomManager> m_intercomManager;

    QPointer<nx::vms::client::desktop::MainWindow> m_mainWindow;

    nx::vms::client::core::UserWatcher* m_userWatcher = nullptr;
    QnWorkbenchLayoutWatcher *m_layoutWatcher = nullptr;

    bool m_closingDown = false;
};

Q_DECLARE_OPAQUE_POINTER(nx::vms::client::desktop::SystemContext*);
