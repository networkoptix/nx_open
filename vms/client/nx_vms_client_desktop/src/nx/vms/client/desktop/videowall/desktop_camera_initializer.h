// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/client_resource_fwd.h>
#include <nx/vms/client/desktop/system_context_aware.h>

namespace nx::vms::client::desktop {

/**
 * Maintains connection between desktop resource and a server we are currently connected to.
 */
class DesktopCameraInitializer:
    public QObject,
    public SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    DesktopCameraInitializer(SystemContext* systemContext, QObject* parent = nullptr);
    virtual ~DesktopCameraInitializer();

private:
    void atResourcesAdded(const QnResourceList& resources);
    void atResourcesRemoved(const QnResourceList& resources);

    /** Setup connection if available. */
    void initialize();

    /** Drop connection. */
    void deinitialize();

    /** Update current server. */
    void setServer(const QnMediaServerResourcePtr &server);

private:
    QnMediaServerResourcePtr m_server;
    core::DesktopResourcePtr m_desktop;
};

} // namespace nx::vms::client::desktop
