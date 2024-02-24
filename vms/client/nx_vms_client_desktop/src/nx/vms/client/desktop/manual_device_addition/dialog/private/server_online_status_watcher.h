// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/utils/scoped_connections.h>

namespace nx::vms::client::desktop {

class ServerOnlineStatusWatcher: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    ServerOnlineStatusWatcher(QObject* parent = nullptr);

    void setServer(const QnMediaServerResourcePtr& server);

    bool isOnline() const;

signals:
    void statusChanged();

private:
    QnMediaServerResourcePtr m_server;
    nx::utils::ScopedConnections m_connections;
};

} // namespace nx::vms::client::desktop
