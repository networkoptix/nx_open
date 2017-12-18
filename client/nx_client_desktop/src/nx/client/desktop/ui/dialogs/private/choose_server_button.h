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

signals:
    void beforeServerChanged();

    void serversCountChanged();

private:
    void updatebuttonData();

    void tryAddServer(const QnResourcePtr& resource);
    void tryRemoveServer(const QnResourcePtr& resource);

    void addMenuItemForServer(const QnMediaServerResourcePtr& server);
    void removeMenuItemForServer(const QnUuid& serverId);

private:
    QnMediaServerResourcePtr m_server;
    QnMediaServerResourceList m_servers;
};
