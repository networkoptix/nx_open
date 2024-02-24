// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/common/widgets/dropdown_button.h>

class QMenu;

class QnChooseServerButton: public DropdownButton
{
    Q_OBJECT
    using base_type = DropdownButton;

public:
    QnChooseServerButton(QWidget* parent = nullptr);

    bool setCurrentServer(const QnMediaServerResourcePtr& currentServer);
    QnMediaServerResourcePtr currentServer() const;

    void addServer(const QnMediaServerResourcePtr& currentServer);
    void removeServer(const nx::Uuid& id);

signals:
    void currentServerChanged(const QnMediaServerResourcePtr& previousServer);

private:
    QAction* addMenuItemForServer(const QnMediaServerResourcePtr& server);

    QAction* actionForServer(const nx::Uuid& id);

    void handleSelectedServerOnlineStatusChanged();

private:
    QnMediaServerResourcePtr m_server;
};
