#pragma once

#include <core/resource/resource_fwd.h>
#include <common/common_module_aware.h>
#include <utils/common/connective.h>

namespace nx {
namespace client {
namespace desktop {

class ServerOnlineStatusWatcher: public Connective<QObject>, public QnCommonModuleAware
{
    Q_OBJECT
    using base_type = Connective<QObject>;

public:
    ServerOnlineStatusWatcher(QObject* parent = nullptr);

    void setServer(const QnMediaServerResourcePtr& server);

    bool isOnline() const;

signals:
    void statusChanged();

private:
    QnMediaServerResourcePtr m_server;
};

} // namespace desktop
} // namespace client
} // namespace nx
