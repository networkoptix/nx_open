// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/core/common/utils/common_module_aware.h>
#include <nx/vms/client/desktop/common/widgets/dropdown_button.h>
#include <nx/utils/uuid.h>

#include "server_online_status_watcher.h"

class QMenu;

class QnChooseServerButton: public DropdownButton, public nx::vms::client::core::CommonModuleAware
{
    Q_OBJECT
    using base_type = DropdownButton;

public:
    QnChooseServerButton(QWidget* parent = nullptr);

    bool setCurrentServer(const QnMediaServerResourcePtr& currentServer);
    QnMediaServerResourcePtr currentServer() const;

    void addServer(const QnMediaServerResourcePtr& currentServer);
    void removeServer(const QnUuid& id);

signals:
    void currentServerChanged(const QnMediaServerResourcePtr& previousServer);

private:
    void tryUpdateCurrentAction();

    QAction* addMenuItemForServer(const QnMediaServerResourcePtr& server);

    QAction* actionForServer(const QnUuid& id);

    void handleSelectedServerOnlineStatusChanged();

private:
    nx::vms::client::desktop::ServerOnlineStatusWatcher m_serverStatus;
    QnMediaServerResourcePtr m_server;
};
