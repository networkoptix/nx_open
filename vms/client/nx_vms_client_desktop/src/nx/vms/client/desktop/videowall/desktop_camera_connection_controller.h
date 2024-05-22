// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/vms/client/core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/system_context_aware.h>

namespace nx::vms::client::desktop {

/**
 * Maintains connection between desktop resource and a server we are currently connected to.
 */
class DesktopCameraConnectionController:
    public QObject,
    public SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    DesktopCameraConnectionController(SystemContext* systemContext, QObject* parent = nullptr);
    virtual ~DesktopCameraConnectionController();

    /** Setup connection if available. */
    void initialize();

    /** Drop and reset connection. */
    void reinitialize();

private:
    void initializeImpl();

    void rememberCamera();

    void atResourcesRemoved(const QnResourceList& resources);
    void atUserChanged(const QnUserResourcePtr& user);

    void disconnectCamera();

    /** Update current server. */
    void setServer(const QnMediaServerResourcePtr& server);

private:
    QnMediaServerResourcePtr m_server;
    core::DesktopResourcePtr m_desktop;
};

} // namespace nx::vms::client::desktop
