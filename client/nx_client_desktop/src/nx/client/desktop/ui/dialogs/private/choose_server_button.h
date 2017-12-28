#pragma once

#include "server_online_status_watcher.h"

#include <ui/widgets/common/dropdown_button.h>
#include <core/resource/resource_fwd.h>
#include <common/common_module_aware.h>
#include <utils/common/connective.h>

class QMenu;
class QnUuid;

class QnChooseServerButton: public Connective<QnDropdownButton>, public QnCommonModuleAware
{
    Q_OBJECT
    using base_type = Connective<QnDropdownButton>;

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
    nx::client::desktop::ServerOnlineStatusWatcher m_serverStatus;
    QnMediaServerResourcePtr m_server;
};
