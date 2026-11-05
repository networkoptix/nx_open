// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/uuid.h>
#include <nx/vms/client/core/resource/resource_fwd.h>
#include <nx/vms/client/core/system_context_aware.h>

namespace nx::client::mobile {

class ServerAudioConnectionWatcher: public QObject, nx::vms::client::core::SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    ServerAudioConnectionWatcher(
        nx::vms::client::core::SystemContext* systemContext,
        QObject* parent = nullptr);

private:
    void tryRemoveCurrentServerConnection();
    void tryAddCurrentServerConnection();

    void handleResourceAdded(const QnResourcePtr& resource);
    void handleResourceRemoved(const QnResourcePtr& resource);

    void updateLegacyDesktopResource();

    void handleRemoteIdChanged();

private:
    nx::Uuid m_remoteServerId;
    nx::vms::client::core::DesktopResourcePtr m_desktop;
    QnMediaServerResourcePtr m_server;
};

} // namespace nx::client::mobile
