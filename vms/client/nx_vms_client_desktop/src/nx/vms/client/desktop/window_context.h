// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>

#include "window_context_aware.h"

class QWidget;

struct QnStartupParameters;
class QnWorkbenchDisplay;
class QnWorkbenchRenderWatcher;
class QnWorkbenchStreamSynchronizer;

namespace nx::vms::client::desktop {

class NotificationActionHandler;
class ShowreelActionsHandler;
class SystemTabBarModel;
class Workbench;
class WorkbenchStateManager;

namespace workbench {

class LocalNotificationsManager;

} // namespace workbench

/**
 * Context of the application window. Created once per each window in scope of the application
 * process. Each Window Context has access to all System Contexts, but only one System Context is
 * considered as "current" - which is used to display Resource Tree and other system-dependent UI
 * elements.
 */
class WindowContext: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString userId READ userId NOTIFY userIdChanged) //< Proxy to system context.
    Q_PROPERTY(QWidget* mainWindow READ mainWindowWidget CONSTANT)
    Q_PROPERTY(nx::vms::client::desktop::SystemContext* systemContext
        READ system WRITE setCurrentSystem NOTIFY systemChanged)

public:
    /**
     * Initialize client-level Window Context.
     * Destruction order must be handled by the caller.
     */
    WindowContext(QObject* parent = nullptr);
    virtual ~WindowContext() override;

    /** Property name to pass Window Context to the QML engine. */
    static QString kQmlPropertyName;
    static void registerQmlType();

    /** Main window. */
    QWidget* mainWindowWidget() const;

    /** System Context, which is selected as current in the given window. */
    SystemContext* system() const;

    /** Choose given system context as a current. */
    void setCurrentSystem(SystemContext* value);

    /**
     * User ID as QString for QML usage. Proxied from current system context to simplify migration.
     */
    QString userId() const;

    Workbench* workbench() const;

    QnWorkbenchContext* workbenchContext() const;

    QnWorkbenchDisplay* display() const;

    QnWorkbenchNavigator* navigator() const;

    /**
     * Manager of menu actions. Creates all actions, which are visible in all context menus (and
     * also some other, which are used for convinience). Provides interface to send action from a
     * single source to any number of receivers using QAction mechanism. Implements complex way of
     * condition checking for those actions.
     */
    menu::Manager* menu() const;

    /**
     * For each Resource Widget on the scene, this class tracks whether it is being currently
     * displayed or not. It provides the necessary getters and signals to track changes of this
     * state.
     */
    QnWorkbenchRenderWatcher* resourceWidgetRenderWatcher() const;

    /**
     * Manages the necessary machinery for synchronized playback of cameras on the scene.
     */
    QnWorkbenchStreamSynchronizer* streamSynchronizer() const;

    SystemTabBarModel* systemTabBarModel() const;

    /** Manages showreel running in the current window, handles corresponding actions.  */
    ShowreelActionsHandler* showreelActionsHandler() const;

    /** Manages event rule & system health notifications. */
    NotificationActionHandler* notificationActionHandler() const;

    /** Manages notifications displayed in the right panel of the window. */
    workbench::LocalNotificationsManager* localNotificationsManager() const;

    /**
     * Asks all registered delegates whether they are ready to disconnect from the system
     * gracefully.
     */
    // DisconnectConfirmer?!!!
    WorkbenchStateManager* workbenchStateManager() const;

    /**
     * Process startup parameters and call related actions.
     */
    void handleStartupParameters(const QnStartupParameters& startupParams);

signals:
    /**
     * Emitted when current System Context is going to be changed.
     *
     * As of current implementation, signal is sent when connection process is initiated (before
     * resources are received), and also when user is disconnecting from the current system (while
     * resources are still present).
     */
    void beforeSystemChanged();

    /**
     * Emitted when current System Context is changed.
     *
     * As of current implementation, signal is sent when initial resources are received. Therefore
     * it can occur several times in the same system in case of reconnection.
     * Also signal is sent when user is completely disconnected from the current system.
     */
    void systemChanged();

    /**
     * Property change notification signal, proxied from current system context to simplify
     * migration to the new contexts architecture. Should be replaced with systemChanged later on.
     */
    void userIdChanged();
private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop

Q_DECLARE_OPAQUE_POINTER(QWidget*);
Q_DECLARE_METATYPE(nx::vms::client::desktop::WindowContext)
Q_DECLARE_OPAQUE_POINTER(nx::vms::client::desktop::SystemContext*);
