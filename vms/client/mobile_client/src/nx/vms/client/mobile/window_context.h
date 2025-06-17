// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/window_context.h>

Q_MOC_INCLUDE("mobile_client/mobile_client_ui_controller.h")
Q_MOC_INCLUDE("nx/vms/client/mobile/maintenance/remote_log_manager.h")
Q_MOC_INCLUDE("nx/vms/client/mobile/session/session_manager.h")
Q_MOC_INCLUDE("nx/vms/client/mobile/system_context.h")
Q_MOC_INCLUDE("nx/vms/client/mobile/ui/ui_controller.h")

class QQuickWindow;
class QnMobileClientUiController;

class QQuickWindow;
class QnTextureSizeHelper;

namespace nx::vms::client::core { class SystemContext; }

namespace nx::vms::client::mobile {

class SessionManager;
class SystemContext;
class RemoteLogManager;
class UiController;

class WindowContext: public core::WindowContext
{
    Q_OBJECT
    using base_type = core::WindowContext;

    Q_PROPERTY(SystemContext* mainSystemContext
        READ mainSystemContext
        NOTIFY mainSystemContextChanged)

    Q_PROPERTY(UiController* ui
        READ uiController
        CONSTANT)

    Q_PROPERTY(SessionManager* sessionManager
        READ sessionManager
        CONSTANT)

    Q_PROPERTY(RemoteLogManager* logManager
        READ logManager
        CONSTANT)

    // Deprecated classes.
    Q_PROPERTY(QnMobileClientUiController* depricatedUiController
        READ depricatedUiController
        CONSTANT)

public:
    WindowContext(QObject* parent = nullptr);
    virtual ~WindowContext() override;

    QQuickWindow* mainWindow() const;

    void setMainSystemContext(SystemContext* context);
    SystemContext* mainSystemContext() const;

    UiController* uiController() const;

    QQuickWindow* window() const;

    SessionManager* sessionManager() const;

    RemoteLogManager* logManager() const;

    // Deprivated classes
    QnMobileClientUiController* depricatedUiController() const;

signals:
    void mainSystemContextChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::mobile
