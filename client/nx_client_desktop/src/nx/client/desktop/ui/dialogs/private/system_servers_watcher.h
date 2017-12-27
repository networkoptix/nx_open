#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <common/common_module_aware.h>
#include <nx/utils/uuid.h>

namespace nx {
namespace client {
namespace desktop {

class SystemServersWatcher: public QObject, public QnCommonModuleAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    SystemServersWatcher(QObject* parent = nullptr);

    QnMediaServerResourceList servers() const;

    int serversCount() const;

signals:
    void serverAdded(const QnMediaServerResourcePtr& server);
    void serverRemoved(const QnUuid& id);
    void serversCountChanged();

private:
    void tryAddServer(const QnResourcePtr& resource);
    void tryRemoveServer(const QnResourcePtr& resource);
    void cleanServers();
    void updateLocalSystemId();

private:
    QnUuid m_localSystemId;
    QnMediaServerResourceList m_servers;
};

} // namespace desktop
} // namespace client
} // namespace nx
