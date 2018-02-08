#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <common/common_module_aware.h>
#include <nx/utils/uuid.h>
#include <utils/common/connective.h>

namespace nx {
namespace client {
namespace desktop {

class CurrentSystemServers: public Connective<QObject>, public QnCommonModuleAware
{
    Q_OBJECT
    using base_type = Connective<QObject>;

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
    void handleFlagsChanged(const QnMediaServerResourcePtr& server);

private:
    QnMediaServerResourceList m_servers;
};

} // namespace desktop
} // namespace client
} // namespace nx
