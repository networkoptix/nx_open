// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/client_resource_fwd.h>
#include <nx/vms/client/core/network/remote_connection_aware.h>
#include <ui/workbench/workbench_state_manager.h>

/**
 * Maintains connection between desktop resource and a server we are currently connected to.
 */
class QnWorkbenchDesktopCameraWatcher:
    public QObject,
    public QnSessionAwareDelegate
{
    Q_OBJECT
    using base_type = QObject;

public:
    QnWorkbenchDesktopCameraWatcher(QObject *parent);
    virtual ~QnWorkbenchDesktopCameraWatcher();

    /** Handle disconnect from server. */
    virtual bool tryClose(bool force) override;

    virtual void forcedUpdate() override;

private:
    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);

    /** Setup connection if available. */
    void initialize();

    /** Drop connection. */
    void deinitialize();

    /** Update current server. */
    void setServer(const QnMediaServerResourcePtr &server);

private:
    QnMediaServerResourcePtr m_server;
    nx::vms::client::core::DesktopResourcePtr m_desktop;
};
