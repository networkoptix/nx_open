// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <common/common_module_aware.h>
#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop {

class CurrentSystemServers: public QObject, public QnCommonModuleAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    CurrentSystemServers(QObject* parent = nullptr);

    QnMediaServerResourceList servers() const;

    int serversCount() const;

signals:
    void serverAdded(const QnMediaServerResourcePtr& server);
    void serverRemoved(const QnUuid& id);
    void serversCountChanged();

private:
    void tryAddServer(const QnResourcePtr& resource);
    void tryRemoveServer(const QnResourcePtr& resource);

private:
    QnMediaServerResourceList m_servers;
};

} // namespace nx::vms::client::desktop
