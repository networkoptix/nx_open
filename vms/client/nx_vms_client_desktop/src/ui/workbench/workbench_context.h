// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <core/resource/resource_fwd.h>
#include <nx/core/watermark/watermark.h>
#include <nx/vms/client/desktop/system_context_aware.h>
#include <nx/vms/client/desktop/window_context_aware.h>
#include <utils/common/instance_storage.h>

class QWidget;
class QnWorkbenchDisplay;

namespace nx::vms::client::core { class UserWatcher; }

namespace nx::vms::client::desktop {

class MainWindow;
class ResourceTreeSettings;
class SettingsDialogManager;
class Workbench;

} // namespace nx::vms::client::desktop

/**
 * This is a class that ties together all objects comprising the global visual scene state.
 * Its instance exists through all application's lifetime.
 */
class QnWorkbenchContext:
    public QObject,
    public QnInstanceStorage,
    public nx::vms::client::desktop::WindowContextAware,
    public nx::vms::client::desktop::SystemContextAware //< TODO: #sivanov Remove this dependency.
{
    Q_OBJECT
    using base_type = QObject;

public:
    QnWorkbenchContext(
        nx::vms::client::desktop::WindowContext* windowContext,
        nx::vms::client::desktop::SystemContext* systemContext,
        QObject* parent = nullptr);
    virtual ~QnWorkbenchContext();

    void initialize();

    QnWorkbenchDisplay* display() const;
    nx::vms::client::desktop::SettingsDialogManager* settingsDialogManager() const;

    nx::vms::client::desktop::MainWindow* mainWindow() const;
    QWidget* mainWindowWidget() const;
    void setMainWindow(nx::vms::client::desktop::MainWindow* mainWindow);

    nx::vms::client::desktop::ResourceTreeSettings* resourceTreeSettings() const;

    nx::core::Watermark watermark() const;

    QnUserResourcePtr user() const;

    /** Check if application is closing down. Replaces QApplication::closingDown(). */
    bool closingDown() const;
    void setClosingDown(bool value);

    /** Whether the scene is visible in the main window. */
    bool isWorkbenchVisible() const;

signals:
    /**
     * This signal is emitted whenever the user that is currently logged in changes.
     *
     * \param user                      New user that was logged in. May be null.
     */
    void userChanged(const QnUserResourcePtr &user);

private:
    QScopedPointer<QnWorkbenchDisplay> m_display;
    QScopedPointer<nx::vms::client::desktop::ResourceTreeSettings> m_resourceTreeSettings;
    std::unique_ptr<nx::vms::client::desktop::SettingsDialogManager> m_settingsDialogManager;

    QPointer<nx::vms::client::desktop::MainWindow> m_mainWindow;

    nx::vms::client::core::UserWatcher* m_userWatcher = nullptr;

    bool m_closingDown = false;
};
