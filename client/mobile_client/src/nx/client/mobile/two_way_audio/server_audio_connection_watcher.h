#pragma once

#include <QtCore/QObject>

#include <nx/utils/uuid.h>

#include <core/resource/client_core_resource_fwd.h>

#include <client_core/connection_context_aware.h>

namespace nx {
namespace client {
namespace mobile {

class ServerAudioConnectionWatcher: public QObject, QnConnectionContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    ServerAudioConnectionWatcher(QObject* parent = nullptr);

private:
    void tryRemoveCurrentServerConnection();

    void tryAddCurrentServerConnection();

    void handleResourceAdded(const QnResourcePtr& resource);

    void handleResourceRemoved(const QnResourcePtr& resource);

    void handleRemoteIdChanged();

private:
    QnUuid m_remoteServerId;
    QnDesktopResourcePtr m_desktop;
    QnMediaServerResourcePtr m_server;
};

} // namespace mobile
} // namespace client
} // namespace nx
