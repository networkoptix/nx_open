// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/system_context_aware.h>

namespace nx::vms::client::desktop {

class CurrentSystemServers:
    public QObject,
    public SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    CurrentSystemServers(SystemContext* systemContext, QObject* parent = nullptr);

    QnMediaServerResourceList servers() const;

    int serversCount() const;

signals:
    void serverAdded(const QnMediaServerResourcePtr& server);
    void serverRemoved(const nx::Uuid& id);
    void serversCountChanged();

private:
    void tryAddServers(const QnResourceList& resources);
    void tryRemoveServers(const QnResourceList& resources);
};

} // namespace nx::vms::client::desktop
