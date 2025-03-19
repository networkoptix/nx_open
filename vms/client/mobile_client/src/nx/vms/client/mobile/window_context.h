// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/window_context.h>

Q_MOC_INCLUDE("nx/vms/client/mobile/session/session_manager.h")
Q_MOC_INCLUDE("nx/vms/client/mobile/system_context.h")

class QQuickWindow;
class QnTextureSizeHelper;

namespace nx::vms::client::core { class SystemContext; }

namespace nx::vms::client::mobile {

class SessionManager;
class SystemContext;

class WindowContext: public core::WindowContext
{
    Q_OBJECT
    using base_type = core::WindowContext;

    Q_PROPERTY(SystemContext* mainSystemContext
        READ mainSystemContext
        NOTIFY mainSystemContextChanged)

    Q_PROPERTY(SessionManager* sessionManager
        READ sessionManager
        CONSTANT)

public:
    WindowContext(QObject* parent = nullptr);
    virtual ~WindowContext() override;

    void initializeWindow();

    QQuickWindow* mainWindow() const;

    QnTextureSizeHelper* textureSizeHelper() const;

    // Returns current system context. Can be nullptr if there is no connection to the system.
    SystemContext* mainSystemContext();

    SessionManager* sessionManager() const;

    SystemContext* createSystemContext();
    void deleteSystemContext(SystemContext* context);

signals:
    void mainSystemContextChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::mobile
