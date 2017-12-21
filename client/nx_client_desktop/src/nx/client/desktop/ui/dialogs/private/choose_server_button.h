#pragma once

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

    bool setServer(const QnMediaServerResourcePtr& server);
    QnMediaServerResourcePtr server() const;

    int serversCount() const;

    bool serverIsOnline();

signals:
    void serverChanged(const QnMediaServerResourcePtr& previousServer);

    void serversCountChanged();

    void serverOnlineStatusChanged();

private:
    void tryUpdateCurrentAction();
    void updatebuttonData();

    void tryAddServer(const QnResourcePtr& resource);
    void tryRemoveServer(const QnResourcePtr& resource);

    QAction* addMenuItemForServer(const QnMediaServerResourcePtr& server);

    QAction* actionForServer(const QnMediaServerResourcePtr& server);

private:
    QnMediaServerResourcePtr m_server;
    QnMediaServerResourceList m_servers;
};
